//
//  RenderableEntityItem.cpp
//  interface/src
//
//  Created by Brad Hefta-Gaub on 12/6/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "RenderableEntityItem.h"

#include <ObjectMotionState.h>

#include "RenderableLightEntityItem.h"
#include "RenderableLineEntityItem.h"
#include "RenderableModelEntityItem.h"
#include "RenderableParticleEffectEntityItem.h"
#include "RenderablePolyVoxEntityItem.h"
#include "RenderablePolyLineEntityItem.h"
#include "RenderableShapeEntityItem.h"
#include "RenderableTextEntityItem.h"
#include "RenderableWebEntityItem.h"
#include "RenderableZoneEntityItem.h"
#include "RenderableMaterialEntityItem.h"
#include "RenderableImageEntityItem.h"
#include "RenderableGridEntityItem.h"


using namespace render;
using namespace render::entities;

// These or the icon "name" used by the render item status value, they correspond to the atlas texture used by the DrawItemStatus
// job in the current rendering pipeline defined as of now  (11/2015) in render-utils/RenderDeferredTask.cpp.
enum class RenderItemStatusIcon {
    ACTIVE_IN_BULLET = 0,
    PACKET_SENT = 1,
    PACKET_RECEIVED = 2,
    SIMULATION_OWNER = 3,
    HAS_ACTIONS = 4,
    OTHER_SIMULATION_OWNER = 5,
    ENTITY_HOST_TYPE = 6,
    NONE = 255
};

void EntityRenderer::initEntityRenderers() {
    REGISTER_ENTITY_TYPE_WITH_FACTORY(Model, RenderableModelEntityItem::factory)
    REGISTER_ENTITY_TYPE_WITH_FACTORY(PolyVox, RenderablePolyVoxEntityItem::factory)
}

const Transform& EntityRenderer::getModelTransform() const {
    return _modelTransform;
}

void EntityRenderer::makeStatusGetters(const EntityItemPointer& entity, Item::Status::Getters& statusGetters) {
    auto nodeList = DependencyManager::get<NodeList>();
    const QUuid& myNodeID = nodeList->getSessionUUID();

    statusGetters.push_back([entity]() -> render::Item::Status::Value {
        uint64_t delta = usecTimestampNow() - entity->getLastEditedFromRemote();
        const float WAIT_THRESHOLD_INV = 1.0f / (0.2f * USECS_PER_SECOND);
        float normalizedDelta = delta * WAIT_THRESHOLD_INV;
        // Status icon will scale from 1.0f down to 0.0f after WAIT_THRESHOLD
        // Color is red if last update is after WAIT_THRESHOLD, green otherwise (120 deg is green)
        return render::Item::Status::Value(1.0f - normalizedDelta, (normalizedDelta > 1.0f ?
            render::Item::Status::Value::GREEN :
            render::Item::Status::Value::RED),
            (unsigned char)RenderItemStatusIcon::PACKET_RECEIVED);
    });

    statusGetters.push_back([entity] () -> render::Item::Status::Value {
        uint64_t delta = usecTimestampNow() - entity->getLastBroadcast();
        const float WAIT_THRESHOLD_INV = 1.0f / (0.4f * USECS_PER_SECOND);
        float normalizedDelta = delta * WAIT_THRESHOLD_INV;
        // Status icon will scale from 1.0f down to 0.0f after WAIT_THRESHOLD
        // Color is Magenta if last update is after WAIT_THRESHOLD, cyan otherwise (180 deg is green)
        return render::Item::Status::Value(1.0f - normalizedDelta, (normalizedDelta > 1.0f ?
            render::Item::Status::Value::MAGENTA :
            render::Item::Status::Value::CYAN),
            (unsigned char)RenderItemStatusIcon::PACKET_SENT);
    });

    statusGetters.push_back([entity] () -> render::Item::Status::Value {
        ObjectMotionState* motionState = static_cast<ObjectMotionState*>(entity->getPhysicsInfo());
        if (motionState && motionState->isActive()) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::BLUE,
                (unsigned char)RenderItemStatusIcon::ACTIVE_IN_BULLET);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::BLUE,
            (unsigned char)RenderItemStatusIcon::ACTIVE_IN_BULLET);
    });

    statusGetters.push_back([entity, myNodeID] () -> render::Item::Status::Value {
        bool weOwnSimulation = entity->getSimulationOwner().matchesValidID(myNodeID);
        bool otherOwnSimulation = !weOwnSimulation && !entity->getSimulationOwner().isNull();

        if (weOwnSimulation) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::BLUE,
                (unsigned char)RenderItemStatusIcon::SIMULATION_OWNER);
        } else if (otherOwnSimulation) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::RED,
                (unsigned char)RenderItemStatusIcon::OTHER_SIMULATION_OWNER);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::BLUE,
            (unsigned char)RenderItemStatusIcon::SIMULATION_OWNER);
    });

    statusGetters.push_back([entity] () -> render::Item::Status::Value {
        if (entity->hasActions()) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::GREEN,
                (unsigned char)RenderItemStatusIcon::HAS_ACTIONS);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::GREEN,
            (unsigned char)RenderItemStatusIcon::HAS_ACTIONS);
    });

    statusGetters.push_back([entity, myNodeID] () -> render::Item::Status::Value {
        if (entity->isAvatarEntity()) {
            if (entity->getOwningAvatarID() == myNodeID) {
                return render::Item::Status::Value(1.0f, render::Item::Status::Value::GREEN,
                    (unsigned char)RenderItemStatusIcon::ENTITY_HOST_TYPE);
            } else {
                return render::Item::Status::Value(1.0f, render::Item::Status::Value::RED,
                    (unsigned char)RenderItemStatusIcon::ENTITY_HOST_TYPE);
            }
        } else if (entity->isLocalEntity()) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::BLUE,
                (unsigned char)RenderItemStatusIcon::ENTITY_HOST_TYPE);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::GREEN,
            (unsigned char)RenderItemStatusIcon::ENTITY_HOST_TYPE);
    });
}


template <typename T> 
std::shared_ptr<T> make_renderer(const EntityItemPointer& entity) {
    // We want to use deleteLater so that renderer destruction gets pushed to the main thread
    return std::shared_ptr<T>(new T(entity), [](T* ptr) { ptr->deleteLater(); });
}

EntityRenderer::EntityRenderer(const EntityItemPointer& entity) : _entity(entity) {
    connect(entity.get(), &EntityItem::requestRenderUpdate, this, [&] {
        _needsRenderUpdate = true;
        emit requestRenderUpdate();
    });
    _materials = entity->getMaterials();
}

EntityRenderer::~EntityRenderer() { }

//
// Smart payload proxy members, implementing the payload interface
//

Item::Bound EntityRenderer::getBound() {
    return _bound;
}

render::hifi::Tag EntityRenderer::getTagMask() const {
    return _isVisibleInSecondaryCamera ? render::hifi::TAG_ALL_VIEWS : render::hifi::TAG_MAIN_VIEW;
}

ItemKey EntityRenderer::getKey() {
    if (isTransparent()) {
        return ItemKey::Builder::transparentShape().withTypeMeta().withTagBits(getTagMask());
    }

    // This allows shapes to cast shadows
    if (_canCastShadow) {
        return ItemKey::Builder::opaqueShape().withTypeMeta().withTagBits(getTagMask()).withShadowCaster();
    } else {
        return ItemKey::Builder::opaqueShape().withTypeMeta().withTagBits(getTagMask());
    }
}

uint32_t EntityRenderer::metaFetchMetaSubItems(ItemIDs& subItems) {
    if (Item::isValidID(_renderItemID)) {
        subItems.emplace_back(_renderItemID);
        return 1;
    }
    return 0;
}

void EntityRenderer::render(RenderArgs* args) {
    if (!isValidRenderItem()) {
        return;
    }

    if (!_renderUpdateQueued && needsRenderUpdate()) {
        // FIXME find a way to spread out the calls to needsRenderUpdate so that only a given subset of the 
        // items checks every frame, like 1/N of the tree ever N frames
        _renderUpdateQueued = true;
        emit requestRenderUpdate();
    }

    auto& renderMode = args->_renderMode;
    bool cauterized = (renderMode != RenderArgs::RenderMode::SHADOW_RENDER_MODE &&
                       renderMode != RenderArgs::RenderMode::SECONDARY_CAMERA_RENDER_MODE &&
                       _cauterized);

    if (_visible && !cauterized) {
        doRender(args);
    }
}

//
// Methods called by the EntityTreeRenderer
//

EntityRenderer::Pointer EntityRenderer::addToScene(EntityTreeRenderer& renderer, const EntityItemPointer& entity, const ScenePointer& scene, Transaction& transaction) {
    EntityRenderer::Pointer result;
    if (!entity) {
        return result;
    }

    using Type = EntityTypes::EntityType_t;
    auto type = entity->getType();
    switch (type) {

        case Type::Shape:
        case Type::Box:
        case Type::Sphere:
            result = make_renderer<ShapeEntityRenderer>(entity);
            break;

        case Type::Model:
            result = make_renderer<ModelEntityRenderer>(entity);
            break;

        case Type::Text:
            result = make_renderer<TextEntityRenderer>(entity);
            break;

        case Type::Image:
            result = make_renderer<ImageEntityRenderer>(entity);
            break;

        case Type::Web:
            if (!nsightActive()) {
                result = make_renderer<WebEntityRenderer>(entity);
            }
            break;

        case Type::ParticleEffect:
            result = make_renderer<ParticleEffectEntityRenderer>(entity);
            break;

        case Type::Line:
            result = make_renderer<LineEntityRenderer>(entity);
            break;

        case Type::PolyLine:
            result = make_renderer<PolyLineEntityRenderer>(entity);
            break;

        case Type::PolyVox:
            result = make_renderer<PolyVoxEntityRenderer>(entity);
            break;

        case Type::Grid:
            result = make_renderer<GridEntityRenderer>(entity);
            break;

        case Type::Light:
            result = make_renderer<LightEntityRenderer>(entity);
            break;

        case Type::Zone:
            result = make_renderer<ZoneEntityRenderer>(entity);
            break;

        case Type::Material:
            result = make_renderer<MaterialEntityRenderer>(entity);
            break;

        default:
            break;
    }

    if (result) {
        result->addToScene(scene, transaction);
    }

    return result;
}

bool EntityRenderer::addToScene(const ScenePointer& scene, Transaction& transaction) {
    _renderItemID = scene->allocateID();
    // Complicated series of trusses
    auto renderPayload = std::make_shared<PayloadProxyInterface::ProxyPayload>(shared_from_this());
    Item::Status::Getters statusGetters;
    makeStatusGetters(_entity, statusGetters);
    renderPayload->addStatusGetters(statusGetters);
    transaction.resetItem(_renderItemID, renderPayload);
    onAddToScene(_entity);
    updateInScene(scene, transaction);
    return true;
}

void EntityRenderer::removeFromScene(const ScenePointer& scene, Transaction& transaction) {
    onRemoveFromScene(_entity);
    transaction.removeItem(_renderItemID);
    Item::clearID(_renderItemID);
}

void EntityRenderer::updateInScene(const ScenePointer& scene, Transaction& transaction) {
    DETAILED_PROFILE_RANGE(simulation_physics, __FUNCTION__);
    if (!isValidRenderItem()) {
        return;
    }
    _updateTime = usecTimestampNow();

    // FIXME is this excessive?
    if (!needsRenderUpdate()) {
        return;
    }

    doRenderUpdateSynchronous(scene, transaction, _entity);
    transaction.updateItem<PayloadProxyInterface>(_renderItemID, [this](PayloadProxyInterface& self) {
        if (!isValidRenderItem()) {
            return;
        }
        // Happens on the render thread.  Classes should use
        doRenderUpdateAsynchronous(_entity);
        _renderUpdateQueued = false;
    });
}

void EntityRenderer::clearSubRenderItemIDs() {
    _subRenderItemIDs.clear();
}

void EntityRenderer::setSubRenderItemIDs(const render::ItemIDs& ids) {
    _subRenderItemIDs = ids;
}

//
// Internal methods
//

// Returns true if the item needs to have updateInscene called because of internal rendering 
// changes (animation, fading, etc)
bool EntityRenderer::needsRenderUpdate() const {
    if (_needsRenderUpdate) {
        return true;
    }

    if (isFading()) {
        return true;
    }

    if (_prevIsTransparent != isTransparent()) {
        return true;
    }
    return needsRenderUpdateFromEntity(_entity);
}

// Returns true if the item in question needs to have updateInScene called because of changes in the entity
bool EntityRenderer::needsRenderUpdateFromEntity(const EntityItemPointer& entity) const {
    bool success = false;
    auto bound = _entity->getAABox(success);
    if (success && _bound != bound) {
        return true;
    }

    auto newModelTransform = _entity->getTransformToCenter(success);
    // FIXME can we use a stale model transform here?
    if (success && newModelTransform != _modelTransform) {
        return true;
    }

    if (_moving != entity->isMovingRelativeToParent()) {
        return true;
    }

    return false;
}

void EntityRenderer::updateModelTransformAndBound() {
    bool success = false;
    auto newModelTransform = _entity->getTransformToCenter(success);
    if (success) {
        _modelTransform = newModelTransform;
    }

    success = false;
    auto bound = _entity->getAABox(success);
    if (success) {
        _bound = bound;
    }
}

void EntityRenderer::doRenderUpdateSynchronous(const ScenePointer& scene, Transaction& transaction, const EntityItemPointer& entity) {
    DETAILED_PROFILE_RANGE(simulation_physics, __FUNCTION__);
    withWriteLock([&] {
        auto transparent = isTransparent();
        auto fading = isFading();
        if (fading || _prevIsTransparent != transparent) {
            emit requestRenderUpdate();
        }
        if (fading) {
            _isFading = Interpolate::calculateFadeRatio(_fadeStartTime) < 1.0f;
        }
        _prevIsTransparent = transparent;

        updateModelTransformAndBound();

        _moving = entity->isMovingRelativeToParent();
        _visible = entity->getVisible();
        setIsVisibleInSecondaryCamera(entity->isVisibleInSecondaryCamera());
        _canCastShadow = entity->getCanCastShadow();
        _cauterized = entity->getCauterized();
        _needsRenderUpdate = false;
    });
}

void EntityRenderer::onAddToScene(const EntityItemPointer& entity) {
    QObject::connect(this, &EntityRenderer::requestRenderUpdate, this, [this] { 
        auto renderer = DependencyManager::get<EntityTreeRenderer>();
        if (renderer) {
            renderer->onEntityChanged(_entity->getID());
        }
    }, Qt::QueuedConnection);
    _changeHandlerId = entity->registerChangeHandler([](const EntityItemID& changedEntity) {
        auto renderer = DependencyManager::get<EntityTreeRenderer>();
        if (renderer) {
            renderer->onEntityChanged(changedEntity);
        }
    });
}

void EntityRenderer::onRemoveFromScene(const EntityItemPointer& entity) { 
    entity->deregisterChangeHandler(_changeHandlerId);
    QObject::disconnect(this, &EntityRenderer::requestRenderUpdate, this, nullptr);
}

void EntityRenderer::addMaterial(graphics::MaterialLayer material, const std::string& parentMaterialName) {
    std::lock_guard<std::mutex> lock(_materialsLock);
    _materials[parentMaterialName].push(material);
}

void EntityRenderer::removeMaterial(graphics::MaterialPointer material, const std::string& parentMaterialName) {
    std::lock_guard<std::mutex> lock(_materialsLock);
    _materials[parentMaterialName].remove(material);
}
