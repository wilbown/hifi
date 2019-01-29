//
//  Created by Bradley Austin Davis on 2016/05/09
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderableShapeEntityItem.h"

#include <glm/gtx/quaternion.hpp>

#include <gpu/Batch.h>
#include <DependencyManager.h>
#include <StencilMaskPass.h>
#include <GeometryCache.h>
#include <PerfStat.h>
#include <shaders/Shaders.h>

#include "RenderPipelines.h"

//#define SHAPE_ENTITY_USE_FADE_EFFECT
#ifdef SHAPE_ENTITY_USE_FADE_EFFECT
#include <FadeEffect.h>
#endif
using namespace render;
using namespace render::entities;

// Sphere entities should fit inside a cube entity of the same size, so a sphere that has dimensions 1x1x1 
// is a half unit sphere.  However, the geometry cache renders a UNIT sphere, so we need to scale down.
static const float SPHERE_ENTITY_SCALE = 0.5f;

static_assert(shader::render_utils::program::simple != 0, "Validate simple program exists");
static_assert(shader::render_utils::program::simple_transparent != 0, "Validate simple transparent program exists");

ShapeEntityRenderer::ShapeEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    _procedural._vertexSource = gpu::Shader::getVertexShaderSource(shader::render_utils::vertex::simple);
    // FIXME: Setup proper uniform slots and use correct pipelines for forward rendering
    _procedural._opaqueFragmentSource = gpu::Shader::Source::get(shader::render_utils::fragment::simple);
    _procedural._transparentFragmentSource = gpu::Shader::Source::get(shader::render_utils::fragment::simple_transparent);

    // TODO: move into Procedural.cpp
    PrepareStencil::testMaskDrawShape(*_procedural._opaqueState);
    PrepareStencil::testMask(*_procedural._transparentState);
}

bool ShapeEntityRenderer::needsRenderUpdate() const {
    if (_procedural.isEnabled() && _procedural.isFading()) {
        return true;
    }

    return Parent::needsRenderUpdate();
}

bool ShapeEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    if (_lastUserData != entity->getUserData()) {
        return true;
    }
    if (_material != entity->getMaterial()) {
        return true;
    }

    if (_shape != entity->getShape()) {
        return true;
    }

    if (_dimensions != entity->getScaledDimensions()) {
        return true;
    }

    return false;
}

void ShapeEntityRenderer::doRenderUpdateSynchronousTyped(const ScenePointer& scene, Transaction& transaction, const TypedEntityPointer& entity) {
    withWriteLock([&] {
        auto userData = entity->getUserData();
        if (_lastUserData != userData) {
            _lastUserData = userData;
            _procedural.setProceduralData(ProceduralData::parse(_lastUserData));
        }

        removeMaterial(_material, "0");
        _material = entity->getMaterial();
        addMaterial(graphics::MaterialLayer(_material, 0), "0");

        _shape = entity->getShape();
    });

    void* key = (void*)this;
    AbstractViewStateInterface::instance()->pushPostUpdateLambda(key, [this] () {
        withWriteLock([&] {
            auto entity = getEntity();
            _position = entity->getWorldPosition();
            _dimensions = entity->getUnscaledDimensions(); // get unscaled to avoid scaling twice
            _orientation = entity->getWorldOrientation();
            updateModelTransformAndBound();
            _renderTransform = getModelTransform(); // contains parent scale, if this entity scales with its parent
            if (_shape == entity::Sphere) {
                _renderTransform.postScale(SPHERE_ENTITY_SCALE);
            }

            _renderTransform.postScale(_dimensions);
        });;
    });
}

void ShapeEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    withReadLock([&] {
        if (_procedural.isEnabled() && _procedural.isFading()) {
            float isFading = Interpolate::calculateFadeRatio(_procedural.getFadeStartTime()) < 1.0f;
            _procedural.setIsFading(isFading);
        }
    });
}

bool ShapeEntityRenderer::isTransparent() const {
    if (_procedural.isEnabled() && _procedural.isFading()) {
        return Interpolate::calculateFadeRatio(_procedural.getFadeStartTime()) < 1.0f;
    }

    auto mat = _materials.find("0");
    if (mat != _materials.end()) {
        if (mat->second.top().material) {
            auto matKey = mat->second.top().material->getKey();
            if (matKey.isTranslucent()) {
                return true;
            }
        }
    }

    return Parent::isTransparent();
}

bool ShapeEntityRenderer::useMaterialPipeline() const {
    bool proceduralReady = resultWithReadLock<bool>([&] {
        return _procedural.isReady();
    });
    if (proceduralReady) {
        return false;
    }

    graphics::MaterialKey drawMaterialKey;
    auto mat = _materials.find("0");
    if (mat != _materials.end() && mat->second.top().material) {
        drawMaterialKey = mat->second.top().material->getKey();
    }

    if (drawMaterialKey.isEmissive() || drawMaterialKey.isUnlit() || drawMaterialKey.isMetallic() || drawMaterialKey.isScattering()) {
        return true;
    }

    // If the material is using any map, we need to use a material ShapeKey
    for (int i = 0; i < graphics::Material::MapChannel::NUM_MAP_CHANNELS; i++) {
        if (drawMaterialKey.isMapChannel(graphics::Material::MapChannel(i))) {
            return true;
        }
    }
    return false;
}

ShapeKey ShapeEntityRenderer::getShapeKey() {
    if (useMaterialPipeline()) {
        graphics::MaterialKey drawMaterialKey;
        if (_materials["0"].top().material) {
            drawMaterialKey = _materials["0"].top().material->getKey();
        }

        bool isTranslucent = drawMaterialKey.isTranslucent();
        bool hasTangents = drawMaterialKey.isNormalMap();
        bool hasLightmap = drawMaterialKey.isLightmapMap();
        bool isUnlit = drawMaterialKey.isUnlit();

        ShapeKey::Builder builder;
        builder.withMaterial();

        if (isTranslucent) {
            builder.withTranslucent();
        }
        if (hasTangents) {
            builder.withTangents();
        }
        if (hasLightmap) {
            builder.withLightmap();
        }
        if (isUnlit) {
            builder.withUnlit();
        }

        return builder.build();
    } else {
        ShapeKey::Builder builder;
        bool proceduralReady = resultWithReadLock<bool>([&] {
            return _procedural.isReady();
        });
        if (proceduralReady) {
            builder.withOwnPipeline();
        }
        if (isTransparent()) {
            builder.withTranslucent();
        }
        return builder.build();
    }
}

void ShapeEntityRenderer::doRender(RenderArgs* args) {
    PerformanceTimer perfTimer("RenderableShapeEntityItem::render");
    Q_ASSERT(args->_batch);

    gpu::Batch& batch = *args->_batch;

    std::shared_ptr<graphics::Material> mat;
    auto geometryCache = DependencyManager::get<GeometryCache>();
    GeometryCache::Shape geometryShape;
    bool proceduralRender = false;
    glm::vec4 outColor;
    withReadLock([&] {
        geometryShape = geometryCache->getShapeForEntityShape(_shape);
        batch.setModelTransform(_renderTransform); // use a transform with scale, rotation, registration point and translation
        mat = _materials["0"].top().material;
        if (mat) {
            outColor = glm::vec4(mat->getAlbedo(), mat->getOpacity());
            if (_procedural.isReady()) {
                outColor = _procedural.getColor(outColor);
                outColor.a *= _procedural.isFading() ? Interpolate::calculateFadeRatio(_procedural.getFadeStartTime()) : 1.0f;
                _procedural.prepare(batch, _position, _dimensions, _orientation, ProceduralProgramKey(outColor.a < 1.0f));
                proceduralRender = true;
            }
        }
    });

    if (!mat) {
        return;
    }

    if (proceduralRender) {
        if (render::ShapeKey(args->_globalShapeKey).isWireframe()) {
            geometryCache->renderWireShape(batch, geometryShape, outColor);
        } else {
            geometryCache->renderShape(batch, geometryShape, outColor);
        }
    } else if (!useMaterialPipeline()) {
        // FIXME, support instanced multi-shape rendering using multidraw indirect
        outColor.a *= _isFading ? Interpolate::calculateFadeRatio(_fadeStartTime) : 1.0f;
        auto pipeline = outColor.a < 1.0f ? geometryCache->getTransparentShapePipeline() : geometryCache->getOpaqueShapePipeline();
        if (render::ShapeKey(args->_globalShapeKey).isWireframe()) {
            geometryCache->renderWireShapeInstance(args, batch, geometryShape, outColor, pipeline);
        } else {
            geometryCache->renderSolidShapeInstance(args, batch, geometryShape, outColor, pipeline);
        }
    } else {
        if (args->_renderMode != render::Args::RenderMode::SHADOW_RENDER_MODE) {
            RenderPipelines::bindMaterial(mat, batch, args->_enableTexturing);
            args->_details._materialSwitches++;
        }

        geometryCache->renderShape(batch, geometryShape);
    }

    const auto triCount = geometryCache->getShapeTriangleCount(geometryShape);
    args->_details._trianglesRendered += (int)triCount;
}

scriptable::ScriptableModelBase ShapeEntityRenderer::getScriptableModel()  {
    scriptable::ScriptableModelBase result;
    auto geometryCache = DependencyManager::get<GeometryCache>();
    auto geometryShape = geometryCache->getShapeForEntityShape(_shape);
    glm::vec3 vertexColor;
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        result.appendMaterials(_materials);
        if (_materials["0"].top().material) {
            vertexColor = _materials["0"].top().material->getAlbedo();
        }
    }
    if (auto mesh = geometryCache->meshFromShape(geometryShape, vertexColor)) {
        result.objectID = getEntity()->getID();
        result.append(mesh);
    }
    return result;
}
