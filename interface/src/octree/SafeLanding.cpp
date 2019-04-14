//
//  SafeLanding.cpp
//  interface/src/octree
//
//  Created by Simon Walton.
//  Copyright 2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "SafeLanding.h"

#include <SharedUtil.h>

#include "EntityTreeRenderer.h"
#include "RenderableModelEntityItem.h"
#include "InterfaceLogging.h"
#include "Application.h"

const int SafeLanding::SEQUENCE_MODULO = std::numeric_limits<OCTREE_PACKET_SEQUENCE>::max() + 1;

namespace {
    template<typename T> bool lessThanWraparound(int a, int b) {
        constexpr int MAX_T_VALUE = std::numeric_limits<T>::max();
        if (b <= a) {
            b += MAX_T_VALUE;
        }
        return (b - a) < (MAX_T_VALUE / 2);
    }
}

bool SafeLanding::SequenceLessThan::operator()(const int& a, const int& b) const {
    return lessThanWraparound<OCTREE_PACKET_SEQUENCE>(a, b);
}

void SafeLanding::startEntitySequence(QSharedPointer<EntityTreeRenderer> entityTreeRenderer) {
    auto entityTree = entityTreeRenderer->getTree();

    if (entityTree) {
        Locker lock(_lock);
        _entityTree = entityTree;
        _trackedEntities.clear();
        _trackingEntities = true;
        _maxTrackedEntityCount = 0;
        connect(std::const_pointer_cast<EntityTree>(_entityTree).get(),
            &EntityTree::addingEntity, this, &SafeLanding::addTrackedEntity);
        connect(std::const_pointer_cast<EntityTree>(_entityTree).get(),
            &EntityTree::deletingEntity, this, &SafeLanding::deleteTrackedEntity);

        _sequenceNumbers.clear();
        _initialStart = INVALID_SEQUENCE;
        _initialEnd = INVALID_SEQUENCE;
        _startTime = usecTimestampNow();
        EntityTreeRenderer::setEntityLoadingPriorityFunction(&ElevatedPriority);
    }
}

void SafeLanding::stopEntitySequence() {
    Locker lock(_lock);
    _trackingEntities = false;
    _maxTrackedEntityCount = 0;
    _trackedEntityStabilityCount = 0;
    _initialStart = INVALID_SEQUENCE;
    _initialEnd = INVALID_SEQUENCE;
    _trackedEntities.clear();
    _sequenceNumbers.clear();
}

void SafeLanding::addTrackedEntity(const EntityItemID& entityID) {
    if (_trackingEntities) {
        Locker lock(_lock);
        EntityItemPointer entity = _entityTree->findEntityByID(entityID);

        if (entity && !entity->isLocalEntity() && entity->getCreated() < _startTime) {

            _trackedEntities.emplace(entityID, entity);
            int trackedEntityCount = (int)_trackedEntities.size();

            if (trackedEntityCount > _maxTrackedEntityCount) {
                _maxTrackedEntityCount = trackedEntityCount;
                _trackedEntityStabilityCount = 0;
            }
            //qCDebug(interfaceapp) << "Safe Landing: Tracking entity " << entity->getItemName();
        }
    }
}

void SafeLanding::deleteTrackedEntity(const EntityItemID& entityID) {
    Locker lock(_lock);
    _trackedEntities.erase(entityID);
}

void SafeLanding::setCompletionSequenceNumbers(int first, int last) {
    Locker lock(_lock);
    if (_initialStart == INVALID_SEQUENCE) {
        _initialStart = first;
        _initialEnd = last;
    }
}

void SafeLanding::noteReceivedsequenceNumber(int sequenceNumber) {
    if (_trackingEntities) {
        Locker lock(_lock);
        _sequenceNumbers.insert(sequenceNumber);
    }
}

bool SafeLanding::isLoadSequenceComplete() {
    if ((isEntityLoadingComplete() && isSequenceNumbersComplete()) || qApp->failedToConnectToEntityServer()) {
        Locker lock(_lock);
        _initialStart = INVALID_SEQUENCE;
        _initialEnd = INVALID_SEQUENCE;
        _entityTree = nullptr;
        _trackingEntities = false; // Don't track anything else that comes in.
        EntityTreeRenderer::setEntityLoadingPriorityFunction(StandardPriority);
    }

    return !_trackingEntities;
}

float SafeLanding::loadingProgressPercentage() {
    Locker lock(_lock);
    static const int MINIMUM_TRACKED_ENTITY_STABILITY_COUNT = 15;

    float entityReadyPercentage = 0.0f;
    if (_maxTrackedEntityCount > 0) {
        entityReadyPercentage = ((_maxTrackedEntityCount - _trackedEntities.size()) / (float)_maxTrackedEntityCount);
    }

    if (_trackedEntityStabilityCount < MINIMUM_TRACKED_ENTITY_STABILITY_COUNT) {
        entityReadyPercentage *= 0.20f;
    }

    return entityReadyPercentage;
}

bool SafeLanding::isSequenceNumbersComplete() {
    if (_initialStart != INVALID_SEQUENCE) {
        Locker lock(_lock);
        int sequenceSize = _initialStart <= _initialEnd ? _initialEnd - _initialStart:
                _initialEnd + SEQUENCE_MODULO - _initialStart;
        auto startIter = _sequenceNumbers.find(_initialStart);
        auto endIter = _sequenceNumbers.find(_initialEnd - 1);

        bool missingSequenceNumbers = qApp->isMissingSequenceNumbers();
        if (sequenceSize == 0 ||
            (startIter != _sequenceNumbers.end()
            && endIter != _sequenceNumbers.end()
             && ((distance(startIter, endIter) == sequenceSize - 1) || !missingSequenceNumbers))) {
            bool enableInterstitial = DependencyManager::get<NodeList>()->getDomainHandler().getInterstitialModeEnabled();
            if (!enableInterstitial) {
                _trackingEntities = false; // Don't track anything else that comes in.
            }
            return true;
        }
    }
    return false;
}

bool isEntityPhysicsReady(const EntityItemPointer& entity) {
    if (entity && !entity->getCollisionless()) {
        const auto& entityType = entity->getType();
        if (entityType == EntityTypes::Model) {
            RenderableModelEntityItem * modelEntity = std::dynamic_pointer_cast<RenderableModelEntityItem>(entity).get();
            static const std::set<ShapeType> downloadedCollisionTypes
                { SHAPE_TYPE_COMPOUND, SHAPE_TYPE_SIMPLE_COMPOUND, SHAPE_TYPE_STATIC_MESH,  SHAPE_TYPE_SIMPLE_HULL };
            bool hasAABox;
            entity->getAABox(hasAABox);
            if (hasAABox && downloadedCollisionTypes.count(modelEntity->getShapeType()) != 0) {
                return (!entity->shouldBePhysical() || entity->isReadyToComputeShape() || modelEntity->computeShapeFailedToLoad());
            }
        }
    }

    return true;
}

bool SafeLanding::isEntityLoadingComplete() {
    Locker lock(_lock);


    auto entityTree = qApp->getEntities();
    auto entityMapIter = _trackedEntities.begin();

    bool enableInterstitial = DependencyManager::get<NodeList>()->getDomainHandler().getInterstitialModeEnabled();

    while (entityMapIter != _trackedEntities.end()) {
        auto entity = entityMapIter->second;

        bool isVisuallyReady = true;

        if (enableInterstitial) {
            auto entityRenderable = entityTree->renderableForEntityId(entityMapIter->first);
            if (!entityRenderable) {
                entityTree->addingEntity(entityMapIter->first);
            }

            isVisuallyReady = entity->isVisuallyReady() || (!entityRenderable && !entity->isParentPathComplete());
        }

        if (isEntityPhysicsReady(entity) && isVisuallyReady) {
            entityMapIter = _trackedEntities.erase(entityMapIter);
        } else {
            if (!isVisuallyReady) {
                entity->requestRenderUpdate();
            }

            entityMapIter++;
        }
    }

    if (enableInterstitial) {
        _trackedEntityStabilityCount++;
    }


    return _trackedEntities.empty();
}

float SafeLanding::ElevatedPriority(const EntityItem& entityItem) {
    return entityItem.getCollisionless() ? 0.0f : 10.0f;
}

void SafeLanding::debugDumpSequenceIDs() const {
    int p = -1;
    qCDebug(interfaceapp) << "Sequence set size:" << _sequenceNumbers.size();
    for (auto s: _sequenceNumbers) {
        if (p == -1) {
            p = s;
            qCDebug(interfaceapp) << "First:" << s;
        } else {
            if (s != p + 1) {
                qCDebug(interfaceapp) << "Gap from" << p << "to" << s << "(exclusive)";
                p = s;
            }
        }
    }
    if (p != -1) {
        qCDebug(interfaceapp) << "Last:" << p;
    }
}
