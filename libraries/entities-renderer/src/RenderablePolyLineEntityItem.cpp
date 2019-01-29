//
//  RenderablePolyLineEntityItem.cpp
//  libraries/entities-renderer/src/
//
//  Created by Eric Levin on 8/10/15
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderablePolyLineEntityItem.h"
#include <ParticleEffectEntityItem.h>

#include <GeometryCache.h>
#include <StencilMaskPass.h>
#include <TextureCache.h>
#include <PathUtils.h>
#include <PerfStat.h>
#include <shaders/Shaders.h>

#include "paintStroke_Shared.slh"

using namespace render;
using namespace render::entities;

gpu::PipelinePointer PolyLineEntityRenderer::_pipeline = nullptr;

static const QUrl DEFAULT_POLYLINE_TEXTURE = PathUtils::resourcesUrl("images/paintStroke.png");

PolyLineEntityRenderer::PolyLineEntityRenderer(const EntityItemPointer& entity) : Parent(entity) {
    _texture = DependencyManager::get<TextureCache>()->getTexture(DEFAULT_POLYLINE_TEXTURE);

    { // Initialize our buffers
        _polylineDataBuffer = std::make_shared<gpu::Buffer>();
        _polylineDataBuffer->resize(sizeof(PolylineData));
        PolylineData data { glm::vec2(_faceCamera, _glow), glm::vec2(0.0f) };
        _polylineDataBuffer->setSubData(0, data);

        _polylineGeometryBuffer = std::make_shared<gpu::Buffer>();
    }
}

void PolyLineEntityRenderer::buildPipeline() {
    // FIXME: opaque pipeline
    gpu::ShaderPointer program = gpu::Shader::createProgram(shader::entities_renderer::program::paintStroke);
    gpu::StatePointer state = gpu::StatePointer(new gpu::State());
    state->setCullMode(gpu::State::CullMode::CULL_NONE);
    state->setDepthTest(true, true, gpu::LESS_EQUAL);
    PrepareStencil::testMask(*state);
    state->setBlendFunction(true,
        gpu::State::SRC_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::INV_SRC_ALPHA,
        gpu::State::FACTOR_ALPHA, gpu::State::BLEND_OP_ADD, gpu::State::ONE);
    _pipeline = gpu::Pipeline::create(program, state);
}

ItemKey PolyLineEntityRenderer::getKey() {
    return ItemKey::Builder::transparentShape().withTypeMeta().withTagBits(getTagMask());
}

ShapeKey PolyLineEntityRenderer::getShapeKey() {
    return ShapeKey::Builder().withOwnPipeline().withTranslucent().withoutCullFace();
}

bool PolyLineEntityRenderer::needsRenderUpdate() const {
    bool textureLoadedChanged = resultWithReadLock<bool>([&] {
        return (!_textureLoaded && _texture && _texture->isLoaded());
    });

    if (textureLoadedChanged) {
        return true;
    }

    return Parent::needsRenderUpdate();
}

bool PolyLineEntityRenderer::needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const {
    return (
        entity->pointsChanged() ||
        entity->widthsChanged() ||
        entity->normalsChanged() ||
        entity->texturesChanged() ||
        entity->colorsChanged() ||
        _isUVModeStretch != entity->getIsUVModeStretch() ||
        _glow != entity->getGlow() ||
        _faceCamera != entity->getFaceCamera()
    );
}

void PolyLineEntityRenderer::doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) {
    auto pointsChanged = entity->pointsChanged();
    auto widthsChanged = entity->widthsChanged();
    auto normalsChanged = entity->normalsChanged();
    auto colorsChanged = entity->colorsChanged();

    bool isUVModeStretch = entity->getIsUVModeStretch();
    bool glow = entity->getGlow();
    bool faceCamera = entity->getFaceCamera();

    entity->resetPolyLineChanged();

    // Transform
    updateModelTransformAndBound();
    _renderTransform = getModelTransform();

    // Textures
    if (entity->texturesChanged()) {
        entity->resetTexturesChanged();
        QUrl entityTextures = DEFAULT_POLYLINE_TEXTURE;
        auto textures = entity->getTextures();
        if (!textures.isEmpty()) {
            entityTextures = QUrl(textures);
        }
        _texture = DependencyManager::get<TextureCache>()->getTexture(entityTextures);
        _textureAspectRatio = 1.0f;
        _textureLoaded = false;
    }

    bool textureChanged = false;
    if (!_textureLoaded && _texture && _texture->isLoaded()) {
        textureChanged = true;
        _textureAspectRatio = (float)_texture->getOriginalHeight() / (float)_texture->getOriginalWidth();
        _textureLoaded = true;
    }

    // Data
    if (faceCamera != _faceCamera || glow != _glow) {
        _faceCamera = faceCamera;
        _glow = glow;
        updateData();
    }

    // Geometry
    if (pointsChanged) {
        _points = entity->getLinePoints();
    }
    if (widthsChanged) {
        _widths = entity->getStrokeWidths();
    }
    if (normalsChanged) {
        _normals = entity->getNormals();
    }
    if (colorsChanged) {
        _colors = entity->getStrokeColors();
        _color = toGlm(entity->getColor());
    }
    if (_isUVModeStretch != isUVModeStretch || pointsChanged || widthsChanged || normalsChanged || colorsChanged || textureChanged) {
        _isUVModeStretch = isUVModeStretch;
        updateGeometry();
    }
}

void PolyLineEntityRenderer::updateGeometry() {
    int maxNumVertices = std::min(_points.length(), _normals.length());

    bool doesStrokeWidthVary = false;
    if (_widths.size() >= 0) {
        for (int i = 1; i < maxNumVertices; i++) {
            float width = PolyLineEntityItem::DEFAULT_LINE_WIDTH;
            if (i < _widths.length()) {
                width = _widths[i];
            }
            if (width != _widths[i - 1]) {
                doesStrokeWidthVary = true;
                break;
            }
        }
    }

    float uCoordInc = 1.0f / maxNumVertices;
    float uCoord = 0.0f;
    float accumulatedDistance = 0.0f;
    float accumulatedStrokeWidth = 0.0f;
    glm::vec3 binormal;

    std::vector<PolylineVertex> vertices;
    vertices.reserve(maxNumVertices);
    for (int i = 0; i < maxNumVertices; i++) {
        // Position
        glm::vec3 point = _points[i];

        // uCoord
        float width = i < _widths.size() ? _widths[i] : PolyLineEntityItem::DEFAULT_LINE_WIDTH;
        if (i > 0) { // First uCoord is 0.0f
            if (!_isUVModeStretch) {
                accumulatedDistance += glm::distance(point, _points[i - 1]);

                if (doesStrokeWidthVary) {
                    //If the stroke varies along the line the texture will stretch more or less depending on the speed
                    //because it looks better than using the same method as below
                    accumulatedStrokeWidth += width;
                    float increaseValue = 1;
                    if (accumulatedStrokeWidth != 0) {
                        float newUcoord = glm::ceil((_textureAspectRatio * accumulatedDistance) / (accumulatedStrokeWidth / i));
                        increaseValue = newUcoord - uCoord;
                    }

                    increaseValue = increaseValue > 0 ? increaseValue : 1;
                    uCoord += increaseValue;
                } else {
                    // If the stroke width is constant then the textures should keep the aspect ratio along the line
                    uCoord = (_textureAspectRatio * accumulatedDistance) / width;
                }
            } else {
                uCoord += uCoordInc;
            }
        }

        // Color
        glm::vec3 color = i < _colors.length() ? _colors[i] : _color;

        // Normal
        glm::vec3 normal = _normals[i];

        // Binormal
        // For last point we can assume binormals are the same since it represents the last two vertices of quad
        if (i < maxNumVertices - 1) {
            glm::vec3 tangent = _points[i + 1] - point;
            binormal = glm::normalize(glm::cross(tangent, normal));

            // Check to make sure binormal is not a NAN. If it is, don't add to vertices vector
            if (binormal.x != binormal.x) {
                continue;
            }
        }

        PolylineVertex vertex = { glm::vec4(point, uCoord), glm::vec4(color, 1.0f), glm::vec4(normal, 0.0f), glm::vec4(binormal, 0.5f * width) };
        vertices.push_back(vertex);
    }

    _numVertices = vertices.size();
    _polylineGeometryBuffer->setData(vertices.size() * sizeof(PolylineVertex), (const gpu::Byte*) vertices.data());
}

void PolyLineEntityRenderer::updateData() {
    PolylineData data { glm::vec2(_faceCamera, _glow), glm::vec2(0.0f) };
    _polylineDataBuffer->setSubData(0, data);
}

void PolyLineEntityRenderer::doRender(RenderArgs* args) {
    if (_numVertices < 2) {
        return;
    }

    PerformanceTimer perfTimer("RenderablePolyLineEntityItem::render");
    Q_ASSERT(args->_batch);
    gpu::Batch& batch = *args->_batch;

    if (!_pipeline) {
        buildPipeline();
    }

    batch.setPipeline(_pipeline);
    batch.setModelTransform(_renderTransform);
    batch.setResourceTexture(0, _textureLoaded ? _texture->getGPUTexture() : DependencyManager::get<TextureCache>()->getWhiteTexture());
    batch.setResourceBuffer(0, _polylineGeometryBuffer);
    batch.setUniformBuffer(0, _polylineDataBuffer);
    batch.draw(gpu::TRIANGLE_STRIP, (gpu::uint32)(2 * _numVertices), 0);
}
