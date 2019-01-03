//
//  EntityTree.cpp
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "EntityTree.h"
#include <QtCore/QDateTime>
#include <QtCore/QQueue>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <NetworkingConstants.h>
#include "AccountManager.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

#include <QtScript/QScriptEngine>

#include <Extents.h>
#include <PerfStat.h>
#include <Profile.h>

#include "EntitySimulation.h"
#include "VariantMapToScriptValue.h"

#include "AddEntityOperator.h"
#include "UpdateEntityOperator.h"
#include "QVariantGLM.h"
#include "EntitiesLogging.h"
#include "RecurseOctreeToMapOperator.h"
#include "RecurseOctreeToJSONOperator.h"
#include "LogHandler.h"
#include "EntityEditFilters.h"
#include "EntityDynamicFactoryInterface.h"

static const quint64 DELETED_ENTITIES_EXTRA_USECS_TO_CONSIDER = USECS_PER_MSEC * 50;
const float EntityTree::DEFAULT_MAX_TMP_ENTITY_LIFETIME = 60 * 60; // 1 hour

// combines the ray cast arguments into a single object
class RayArgs {
public:
    // Inputs
    glm::vec3 origin;
    glm::vec3 direction;
    glm::vec3 invDirection;
    const QVector<EntityItemID>& entityIdsToInclude;
    const QVector<EntityItemID>& entityIdsToDiscard;
    bool visibleOnly;
    bool collidableOnly;
    bool precisionPicking;

    // Outputs
    OctreeElementPointer& element;
    float& distance;
    BoxFace& face;
    glm::vec3& surfaceNormal;
    QVariantMap& extraInfo;
    EntityItemID entityID;
};

class ParabolaArgs {
public:
    // Inputs
    glm::vec3 origin;
    glm::vec3 velocity;
    glm::vec3 acceleration;
    const QVector<EntityItemID>& entityIdsToInclude;
    const QVector<EntityItemID>& entityIdsToDiscard;
    bool visibleOnly;
    bool collidableOnly;
    bool precisionPicking;

    // Outputs
    OctreeElementPointer& element;
    float& parabolicDistance;
    BoxFace& face;
    glm::vec3& surfaceNormal;
    QVariantMap& extraInfo;
    EntityItemID entityID;
};


EntityTree::EntityTree(bool shouldReaverage) :
    Octree(shouldReaverage)
{
    resetClientEditStats();

    EntityItem::retrieveMarketplacePublicKey();
}

EntityTree::~EntityTree() {
    eraseAllOctreeElements(false);
}

void EntityTree::setEntityScriptSourceWhitelist(const QString& entityScriptSourceWhitelist) { 
    _entityScriptSourceWhitelist = entityScriptSourceWhitelist.split(',', QString::SkipEmptyParts);
}


void EntityTree::createRootElement() {
    _rootElement = createNewElement();
}

OctreeElementPointer EntityTree::createNewElement(unsigned char* octalCode) {
    auto newElement = EntityTreeElementPointer(new EntityTreeElement(octalCode));
    newElement->setTree(std::static_pointer_cast<EntityTree>(shared_from_this()));
    return std::static_pointer_cast<OctreeElement>(newElement);
}

void EntityTree::eraseAllOctreeElements(bool createNewRoot) {
    emit clearingEntities();

    if (_simulation) {
        _simulation->clearEntities();
    }
    _staleProxies.clear();
    QHash<EntityItemID, EntityItemPointer> localMap;
    localMap.swap(_entityMap);
    this->withWriteLock([&] {
        foreach(EntityItemPointer entity, localMap) {
            EntityTreeElementPointer element = entity->getElement();
            if (element) {
                element->cleanupEntities();
            }
        }
    });
    localMap.clear();
    Octree::eraseAllOctreeElements(createNewRoot);

    resetClientEditStats();
    clearDeletedEntities();

    {
        QWriteLocker locker(&_needsParentFixupLock);
        _needsParentFixup.clear();
    }
}

void EntityTree::readBitstreamToTree(const unsigned char* bitstream,
            uint64_t bufferSizeBytes, ReadBitstreamToTreeParams& args) {
    Octree::readBitstreamToTree(bitstream, bufferSizeBytes, args);

    // add entities
    QHash<EntityItemID, EntityItemPointer>::const_iterator itr;
    for (itr = _entitiesToAdd.constBegin(); itr != _entitiesToAdd.constEnd(); ++itr) {
        const EntityItemPointer& entityItem = itr.value();
        AddEntityOperator theOperator(getThisPointer(), entityItem);
        recurseTreeWithOperator(&theOperator);
        postAddEntity(entityItem);
    }
    _entitiesToAdd.clear();

    // move entities
    if (_entityMover.hasMovingEntities()) {
        PerformanceTimer perfTimer("recurseTreeWithOperator");
        recurseTreeWithOperator(&_entityMover);
        _entityMover.reset();
    }
}

int EntityTree::readEntityDataFromBuffer(const unsigned char* data, int bytesLeftToRead, ReadBitstreamToTreeParams& args) {
    const unsigned char* dataAt = data;
    int bytesRead = 0;
    uint16_t numberOfEntities = 0;
    int expectedBytesPerEntity = EntityItem::expectedBytes();

    args.elementsPerPacket++;

    if (bytesLeftToRead >= (int)sizeof(numberOfEntities)) {
        // read our entities in....
        numberOfEntities = *(uint16_t*)dataAt;

        dataAt += sizeof(numberOfEntities);
        bytesLeftToRead -= (int)sizeof(numberOfEntities);
        bytesRead += sizeof(numberOfEntities);

        if (bytesLeftToRead >= (int)(numberOfEntities * expectedBytesPerEntity)) {
            for (uint16_t i = 0; i < numberOfEntities; i++) {
                int bytesForThisEntity = 0;
                EntityItemID entityItemID = EntityItemID::readEntityItemIDFromBuffer(dataAt, bytesLeftToRead);
                EntityItemPointer entity = findEntityByEntityItemID(entityItemID);

                if (entity) {
                    QString entityScriptBefore = entity->getScript();
                    QUuid parentIDBefore = entity->getParentID();
                    QString entityServerScriptsBefore = entity->getServerScripts();
                    quint64 entityScriptTimestampBefore = entity->getScriptTimestamp();

                    bytesForThisEntity = entity->readEntityDataFromBuffer(dataAt, bytesLeftToRead, args);
                    if (entity->getDirtyFlags()) {
                        entityChanged(entity);
                    }
                    _entityMover.addEntityToMoveList(entity, entity->getQueryAACube());

                    QString entityScriptAfter = entity->getScript();
                    QString entityServerScriptsAfter = entity->getServerScripts();
                    quint64 entityScriptTimestampAfter = entity->getScriptTimestamp();
                    bool reload = entityScriptTimestampBefore != entityScriptTimestampAfter;

                    // If the script value has changed on us, or it's timestamp has changed to force
                    // a reload then we want to send out a script changing signal...
                    if (reload || entityScriptBefore != entityScriptAfter) {
                        emitEntityScriptChanging(entityItemID, reload); // the entity script has changed
                    }
                    if (reload || entityServerScriptsBefore != entityServerScriptsAfter) {
                        emitEntityServerScriptChanging(entityItemID, reload); // the entity server script has changed
                    }

                    QUuid parentIDAfter = entity->getParentID();
                    if (parentIDBefore != parentIDAfter) {
                        addToNeedsParentFixupList(entity);
                    }
                } else {
                    entity = EntityTypes::constructEntityItem(dataAt, bytesLeftToRead, args);
                    if (entity) {
                        bytesForThisEntity = entity->readEntityDataFromBuffer(dataAt, bytesLeftToRead, args);

                        // don't add if we've recently deleted....
                        if (!isDeletedEntity(entityItemID)) {
                            _entitiesToAdd.insert(entityItemID, entity);

                            if (entity->getCreated() == UNKNOWN_CREATED_TIME) {
                                entity->recordCreationTime();
                            }
                        #ifdef WANT_DEBUG
                        } else {
                                qCDebug(entities) << "Received packet for previously deleted entity [" <<
                                        entityItemID << "] ignoring. (inside " << __FUNCTION__ << ")";
                        #endif
                        }
                    }
                }
                // Move the buffer forward to read more entities
                dataAt += bytesForThisEntity;
                bytesLeftToRead -= bytesForThisEntity;
                bytesRead += bytesForThisEntity;
            }
        }
    }

    return bytesRead;
}

bool EntityTree::handlesEditPacketType(PacketType packetType) const {
    // we handle these types of "edit" packets
    switch (packetType) {
        case PacketType::EntityAdd:
        case PacketType::EntityClone:
        case PacketType::EntityEdit:
        case PacketType::EntityErase:
        case PacketType::EntityPhysics:
            return true;
        default:
            return false;
    }
}

/// Adds a new entity item to the tree
void EntityTree::postAddEntity(EntityItemPointer entity) {
    assert(entity);

    if (getIsServer()) {
        QString certID(entity->getCertificateID());
        EntityItemID entityItemID = entity->getEntityItemID();
        EntityItemID existingEntityItemID;

        {
            QWriteLocker locker(&_entityCertificateIDMapLock);
            existingEntityItemID = _entityCertificateIDMap.value(certID);
            if (!certID.isEmpty()) {
                _entityCertificateIDMap.insert(certID, entityItemID);
                qCDebug(entities) << "Certificate ID" << certID << "belongs to" << entityItemID;
            }
        }

        // Delete an already-existing entity from the tree if it has the same
        //     CertificateID as the entity we're trying to add.
        if (!existingEntityItemID.isNull()) {
            qCDebug(entities) << "Certificate ID" << certID << "already exists on entity with ID"
                << existingEntityItemID << ". Deleting existing entity.";
            deleteEntity(existingEntityItemID, true);
            return;
        }
    }

    // check to see if we need to simulate this entity..
    if (_simulation) {
        _simulation->addEntity(entity);
    }

    if (!entity->getParentID().isNull()) {
        addToNeedsParentFixupList(entity);
    }

    _isDirty = true;

    // find and hook up any entities with this entity as a (previously) missing parent
    fixupNeedsParentFixups();

    emit addingEntity(entity->getEntityItemID());
    emit addingEntityPointer(entity.get());
}

bool EntityTree::updateEntity(const EntityItemID& entityID, const EntityItemProperties& properties, const SharedNodePointer& senderNode) {
    EntityItemPointer entity;
    {
        QReadLocker locker(&_entityMapLock);
        entity = _entityMap.value(entityID);
    }
    if (!entity) {
        return false;
    }
    return updateEntity(entity, properties, senderNode);
}

bool EntityTree::updateEntity(EntityItemPointer entity, const EntityItemProperties& origProperties,
        const SharedNodePointer& senderNode) {
    EntityTreeElementPointer containingElement = entity->getElement();
    if (!containingElement) {
        return false;
    }

    EntityItemProperties properties = origProperties;

    bool allowLockChange;
    QUuid senderID;
    if (senderNode.isNull()) {
        auto nodeList = DependencyManager::get<NodeList>();
        allowLockChange = nodeList->isAllowedEditor();
        senderID = nodeList->getSessionUUID();
    } else {
        allowLockChange = senderNode->isAllowedEditor();
        senderID = senderNode->getUUID();
    }

    if (!allowLockChange && (entity->getLocked() != properties.getLocked())) {
        qCDebug(entities) << "Refusing disallowed lock adjustment.";
        return false;
    }

    // enforce support for locked entities. If an entity is currently locked, then the only
    // property we allow you to change is the locked property.
    if (entity->getLocked()) {
        if (properties.lockedChanged()) {
            bool wantsLocked = properties.getLocked();
            if (!wantsLocked) {
                EntityItemProperties tempProperties;
                tempProperties.setLocked(wantsLocked);
                tempProperties.setLastEdited(properties.getLastEdited());

                bool success;
                AACube queryCube = entity->getQueryAACube(success);
                if (!success) {
                    qCWarning(entities) << "failed to get query-cube for" << entity->getID();
                }
                UpdateEntityOperator theOperator(getThisPointer(), containingElement, entity, queryCube);
                recurseTreeWithOperator(&theOperator);
                if (entity->setProperties(tempProperties)) {
                    emit editingEntityPointer(entity);
                }
                _isDirty = true;
            }
        }
    } else {
        if (getIsServer()) {
            bool simulationBlocked = !entity->getSimulatorID().isNull();
            if (properties.simulationOwnerChanged()) {
                QUuid submittedID = properties.getSimulationOwner().getID();
                // a legit interface will only submit their own ID or NULL:
                if (submittedID.isNull()) {
                    if (entity->getSimulatorID() == senderID) {
                        // We only allow the simulation owner to clear their own simulationID's.
                        simulationBlocked = false;
                        properties.clearSimulationOwner(); // clear everything
                    }
                    // else: We assume the sender really did believe it was the simulation owner when it sent
                } else if (submittedID == senderID) {
                    // the sender is trying to take or continue ownership
                    if (entity->getSimulatorID().isNull()) {
                        // the sender is taking ownership
                        if (properties.getSimulationOwner().getPriority() == VOLUNTEER_SIMULATION_PRIORITY) {
                            // the entity-server always promotes VOLUNTEER to RECRUIT to avoid ownership thrash
                            // when dynamic objects first activate and multiple participants bid simultaneously
                            properties.setSimulationPriority(RECRUIT_SIMULATION_PRIORITY);
                        }
                        simulationBlocked = false;
                    } else if (entity->getSimulatorID() == senderID) {
                        // the sender is asserting ownership, maybe changing priority
                        simulationBlocked = false;
                        // the entity-server always promotes VOLUNTEER to RECRUIT to avoid ownership thrash
                        // when dynamic objects first activate and multiple participants bid simultaneously
                        if (properties.getSimulationOwner().getPriority() == VOLUNTEER_SIMULATION_PRIORITY) {
                            properties.setSimulationPriority(RECRUIT_SIMULATION_PRIORITY);
                        }
                    } else {
                        // the sender is trying to steal ownership from another simulator
                        // so we apply the rules for ownership change:
                        // (1) higher priority wins
                        // (2) equal priority wins if ownership filter has expired
                        // (3) VOLUNTEER priority is promoted to RECRUIT
                        uint8_t oldPriority = entity->getSimulationPriority();
                        uint8_t newPriority = properties.getSimulationOwner().getPriority();
                        if (newPriority > oldPriority ||
                             (newPriority == oldPriority && properties.getSimulationOwner().hasExpired())) {
                            simulationBlocked = false;
                            if (properties.getSimulationOwner().getPriority() == VOLUNTEER_SIMULATION_PRIORITY) {
                                properties.setSimulationPriority(RECRUIT_SIMULATION_PRIORITY);
                            }
                        }
                    }
                    if (!simulationBlocked) {
                        entity->setSimulationOwnershipExpiry(usecTimestampNow() + MAX_INCOMING_SIMULATION_UPDATE_PERIOD);
                    }
                } else {
                    // the entire update is suspect --> ignore it
                    return false;
                }
            } else if (simulationBlocked) {
                simulationBlocked = senderID != entity->getSimulatorID();
                if (!simulationBlocked) {
                    entity->setSimulationOwnershipExpiry(usecTimestampNow() + MAX_INCOMING_SIMULATION_UPDATE_PERIOD);
                }
            }
            if (simulationBlocked) {
                // squash ownership and physics-related changes.
                // TODO? replace these eight calls with just one?
                properties.setSimulationOwnerChanged(false);
                properties.setPositionChanged(false);
                properties.setRotationChanged(false);
                properties.setVelocityChanged(false);
                properties.setAngularVelocityChanged(false);
                properties.setAccelerationChanged(false);
                properties.setParentIDChanged(false);
                properties.setParentJointIndexChanged(false);

                if (wantTerseEditLogging()) {
                    qCDebug(entities) << (senderNode ? senderNode->getUUID() : "null") << "physical edits suppressed";
                }
            }
        }
        // else client accepts what the server says

        QString entityScriptBefore = entity->getScript();
        quint64 entityScriptTimestampBefore = entity->getScriptTimestamp();
        uint32_t preFlags = entity->getDirtyFlags();

        AACube newQueryAACube;
        if (properties.queryAACubeChanged()) {
            newQueryAACube = properties.getQueryAACube();
        } else {
            newQueryAACube = entity->getQueryAACube();
        }
        UpdateEntityOperator theOperator(getThisPointer(), containingElement, entity, newQueryAACube);
        recurseTreeWithOperator(&theOperator);
        if (entity->setProperties(properties)) {
            emit editingEntityPointer(entity);
        }

        // if the entity has children, run UpdateEntityOperator on them.  If the children have children, recurse
        QQueue<SpatiallyNestablePointer> toProcess;
        foreach (SpatiallyNestablePointer child, entity->getChildren()) {
            if (child && child->getNestableType() == NestableType::Entity) {
                toProcess.enqueue(child);
            }
        }

        while (!toProcess.empty()) {
            EntityItemPointer childEntity = std::static_pointer_cast<EntityItem>(toProcess.dequeue());
            if (!childEntity) {
                continue;
            }
            EntityTreeElementPointer childContainingElement = childEntity->getElement();
            if (!childContainingElement) {
                continue;
            }

            bool success;
            AACube queryCube = childEntity->getQueryAACube(success);
            if (!success) {
                addToNeedsParentFixupList(childEntity);
                continue;
            }
            if (!childEntity->getParentID().isNull()) {
                addToNeedsParentFixupList(childEntity);
            }

            UpdateEntityOperator theChildOperator(getThisPointer(), childContainingElement, childEntity, queryCube);
            recurseTreeWithOperator(&theChildOperator);
            foreach (SpatiallyNestablePointer childChild, childEntity->getChildren()) {
                if (childChild && childChild->getNestableType() == NestableType::Entity) {
                    toProcess.enqueue(childChild);
                }
            }
        }

        _isDirty = true;

        uint32_t newFlags = entity->getDirtyFlags() & ~preFlags;
        if (newFlags) {
            if (entity->isSimulated()) {
                assert((bool)_simulation);
                if (newFlags & DIRTY_SIMULATION_FLAGS) {
                    _simulation->changeEntity(entity);
                }
            } else {
                // normally the _simulation clears ALL dirtyFlags, but when not possible we do it explicitly
                entity->clearDirtyFlags();
            }
        }

        QString entityScriptAfter = entity->getScript();
        quint64 entityScriptTimestampAfter = entity->getScriptTimestamp();
        bool reload = entityScriptTimestampBefore != entityScriptTimestampAfter;
        if (entityScriptBefore != entityScriptAfter || reload) {
            emitEntityScriptChanging(entity->getEntityItemID(), reload); // the entity script has changed
        }
    }

    // TODO: this final containingElement check should eventually be removed (or wrapped in an #ifdef DEBUG).
    if (!entity->getElement()) {
        qCWarning(entities) << "EntityTree::updateEntity() we no longer have a containing element for entityID="
                << entity->getEntityItemID();
        return false;
    }

    return true;
}

EntityItemPointer EntityTree::addEntity(const EntityItemID& entityID, const EntityItemProperties& properties, bool isClone) {
    EntityItemPointer result = NULL;
    EntityItemProperties props = properties;

    auto nodeList = DependencyManager::get<NodeList>();
    if (!nodeList) {
        qCDebug(entities) << "EntityTree::addEntity -- can't get NodeList";
        return nullptr;
    }

    if (properties.getEntityHostType() == entity::HostType::DOMAIN && getIsClient() &&
        !nodeList->getThisNodeCanRez() && !nodeList->getThisNodeCanRezTmp() &&
        !nodeList->getThisNodeCanRezCertified() && !nodeList->getThisNodeCanRezTmpCertified() && !_serverlessDomain && !isClone) {
        return nullptr;
    }

    bool recordCreationTime = false;
    if (props.getCreated() == UNKNOWN_CREATED_TIME) {
        // the entity's creation time was not specified in properties, which means this is a NEW entity
        // and we must record its creation time
        recordCreationTime = true;
    }

    // You should not call this on existing entities that are already part of the tree! Call updateEntity()
    EntityTreeElementPointer containingElement = getContainingElement(entityID);
    if (containingElement) {
        qCWarning(entities) << "EntityTree::addEntity() on existing entity item with entityID=" << entityID
                          << "containingElement=" << containingElement.get();
        return result;
    }

    // construct the instance of the entity
    EntityTypes::EntityType type = props.getType();
    result = EntityTypes::constructEntityItem(type, entityID, props);

    if (result) {
        if (recordCreationTime) {
            result->recordCreationTime();
        }
        // Recurse the tree and store the entity in the correct tree element
        AddEntityOperator theOperator(getThisPointer(), result);
        recurseTreeWithOperator(&theOperator);
        if (!result->getParentID().isNull()) {
            addToNeedsParentFixupList(result);
        }

        postAddEntity(result);
    }
    return result;
}

void EntityTree::emitEntityScriptChanging(const EntityItemID& entityItemID, bool reload) {
    emit entityScriptChanging(entityItemID, reload);
}

void EntityTree::emitEntityServerScriptChanging(const EntityItemID& entityItemID, bool reload) {
    emit entityServerScriptChanging(entityItemID, reload);
}

void EntityTree::notifyNewCollisionSoundURL(const QString& newURL, const EntityItemID& entityID) {
    emit newCollisionSoundURL(QUrl(newURL), entityID);
}

void EntityTree::setSimulation(EntitySimulationPointer simulation) {
    this->withWriteLock([&] {
        if (simulation) {
            // assert that the simulation's backpointer has already been properly connected
            assert(simulation->getEntityTree().get() == this);
        }
        if (_simulation && _simulation != simulation) {
            _simulation->clearEntities();
        }
        _simulation = simulation;
    });
}

void EntityTree::deleteEntity(const EntityItemID& entityID, bool force, bool ignoreWarnings) {
    EntityTreeElementPointer containingElement = getContainingElement(entityID);
    if (!containingElement) {
        if (!ignoreWarnings) {
            qCWarning(entities) << "EntityTree::deleteEntity() on non-existent entityID=" << entityID;
        }
        return;
    }

    EntityItemPointer existingEntity = containingElement->getEntityWithEntityItemID(entityID);
    if (!existingEntity) {
        if (!ignoreWarnings) {
            qCWarning(entities) << "EntityTree::deleteEntity() on non-existant entity item with entityID=" << entityID;
        }
        return;
    }

    if (existingEntity->getLocked() && !force) {
        if (!ignoreWarnings) {
            qCDebug(entities) << "ERROR! EntityTree::deleteEntity() trying to delete locked entity. entityID=" << entityID;
        }
        return;
    }

    cleanupCloneIDs(entityID);
    unhookChildAvatar(entityID);
    emit deletingEntity(entityID);
    emit deletingEntityPointer(existingEntity.get());

    // NOTE: callers must lock the tree before using this method
    DeleteEntityOperator theOperator(getThisPointer(), entityID);

    existingEntity->forEachDescendant([&](SpatiallyNestablePointer descendant) {
        auto descendantID = descendant->getID();
        theOperator.addEntityIDToDeleteList(descendantID);
        emit deletingEntity(descendantID);
        EntityItemPointer descendantEntity = std::dynamic_pointer_cast<EntityItem>(descendant);
        if (descendantEntity) {
            emit deletingEntityPointer(descendantEntity.get());
        }
    });

    recurseTreeWithOperator(&theOperator);
    processRemovedEntities(theOperator);
    _isDirty = true;
}

void EntityTree::unhookChildAvatar(const EntityItemID entityID) {

    EntityItemPointer entity = findEntityByEntityItemID(entityID);

    entity->forEachDescendant([&](SpatiallyNestablePointer child) {
        if (child->getNestableType() == NestableType::Avatar) {
            child->setParentID(nullptr);
        }
    });
}

void EntityTree::cleanupCloneIDs(const EntityItemID& entityID) {
    EntityItemPointer entity = findEntityByEntityItemID(entityID);
    if (entity) {
        // remove clone ID from its clone origin's clone ID list if clone origin exists
        const QUuid& cloneOriginID = entity->getCloneOriginID();
        if (!cloneOriginID.isNull()) {
            EntityItemPointer cloneOrigin = findEntityByID(cloneOriginID);
            if (cloneOrigin) {
                cloneOrigin->removeCloneID(entityID);
            }
        }
        // clear the clone origin ID on any clones that this entity had
        const QVector<QUuid>& cloneIDs = entity->getCloneIDs();
        foreach(const QUuid& cloneChildID, cloneIDs) {
            EntityItemPointer cloneChild = findEntityByEntityItemID(cloneChildID);
            if (cloneChild) {
                cloneChild->setCloneOriginID(QUuid());
            }
        }
    }
}

void EntityTree::deleteEntities(QSet<EntityItemID> entityIDs, bool force, bool ignoreWarnings) {
    // NOTE: callers must lock the tree before using this method
    DeleteEntityOperator theOperator(getThisPointer());
    foreach(const EntityItemID& entityID, entityIDs) {
        EntityTreeElementPointer containingElement = getContainingElement(entityID);
        if (!containingElement) {
            if (!ignoreWarnings) {
                qCWarning(entities) << "EntityTree::deleteEntities() on non-existent entityID=" << entityID;
            }
            continue;
        }

        EntityItemPointer existingEntity = containingElement->getEntityWithEntityItemID(entityID);
        if (!existingEntity) {
            if (!ignoreWarnings) {
                qCWarning(entities) << "EntityTree::deleteEntities() on non-existent entity item with entityID=" << entityID;
            }
            continue;
        }

        if (existingEntity->getLocked() && !force) {
            if (!ignoreWarnings) {
                qCDebug(entities) << "ERROR! EntityTree::deleteEntities() trying to delete locked entity. entityID=" << entityID;
            }
            continue;
        }

        // tell our delete operator about this entityID
        cleanupCloneIDs(entityID);
        unhookChildAvatar(entityID);
        theOperator.addEntityIDToDeleteList(entityID);
        emit deletingEntity(entityID);
        emit deletingEntityPointer(existingEntity.get());
    }

    if (!theOperator.getEntities().empty()) {
        recurseTreeWithOperator(&theOperator);
        processRemovedEntities(theOperator);
        _isDirty = true;
    }
}

void EntityTree::processRemovedEntities(const DeleteEntityOperator& theOperator) {
    quint64 deletedAt = usecTimestampNow();
    const RemovedEntities& entities = theOperator.getEntities();
    foreach(const EntityToDeleteDetails& details, entities) {
        EntityItemPointer theEntity = details.entity;

        if (getIsServer()) {
            QSet<EntityItemID> childrenIDs;
            theEntity->forEachChild([&](SpatiallyNestablePointer child) {
                if (child->getNestableType() == NestableType::Entity) {
                    childrenIDs += child->getID();
                }
            });
            deleteEntities(childrenIDs, true, true);
        }

        theEntity->die();

        if (getIsServer()) {
            {
                QWriteLocker entityCertificateIDMapLocker(&_entityCertificateIDMapLock);
                QString certID = theEntity->getCertificateID();
                if (theEntity->getEntityItemID() == _entityCertificateIDMap.value(certID)) {
                    _entityCertificateIDMap.remove(certID);
                }
            }

            // set up the deleted entities ID
            QWriteLocker recentlyDeletedEntitiesLocker(&_recentlyDeletedEntitiesLock);
            _recentlyDeletedEntityItemIDs.insert(deletedAt, theEntity->getEntityItemID());
        } else {
            // on the client side, we also remember that we deleted this entity, we don't care about the time
            trackDeletedEntity(theEntity->getEntityItemID());
        }

        if (theEntity->isSimulated()) {
            _simulation->prepareEntityForDelete(theEntity);
        }

        // keep a record of valid stale spaceIndices so they can be removed from the Space
        int32_t spaceIndex = theEntity->getSpaceIndex();
        if (spaceIndex != -1) {
            _staleProxies.push_back(spaceIndex);
        }
    }
}


class FindNearPointArgs {
public:
    glm::vec3 position;
    float targetRadius;
    bool found;
    EntityItemPointer closestEntity;
    float closestEntityDistance;
};


bool EntityTree::findNearPointOperation(const OctreeElementPointer& element, void* extraData) {
    FindNearPointArgs* args = static_cast<FindNearPointArgs*>(extraData);
    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);

    glm::vec3 penetration;
    bool sphereIntersection = entityTreeElement->getAACube().findSpherePenetration(args->position, args->targetRadius, penetration);

    // If this entityTreeElement contains the point, then search it...
    if (sphereIntersection) {
        EntityItemPointer thisClosestEntity = entityTreeElement->getClosestEntity(args->position);

        // we may have gotten NULL back, meaning no entity was available
        if (thisClosestEntity) {
            glm::vec3 entityPosition = thisClosestEntity->getWorldPosition();
            float distanceFromPointToEntity = glm::distance(entityPosition, args->position);

            // If we're within our target radius
            if (distanceFromPointToEntity <= args->targetRadius) {
                // we are closer than anything else we've found
                if (distanceFromPointToEntity < args->closestEntityDistance) {
                    args->closestEntity = thisClosestEntity;
                    args->closestEntityDistance = distanceFromPointToEntity;
                    args->found = true;
                }
            }
        }

        // we should be able to optimize this...
        return true; // keep searching in case children have closer entities
    }

    // if this element doesn't contain the point, then none of its children can contain the point, so stop searching
    return false;
}

bool findRayIntersectionOp(const OctreeElementPointer& element, void* extraData) {
    RayArgs* args = static_cast<RayArgs*>(extraData);
    bool keepSearching = true;
    EntityTreeElementPointer entityTreeElementPointer = std::static_pointer_cast<EntityTreeElement>(element);
    EntityItemID entityID = entityTreeElementPointer->findRayIntersection(args->origin, args->direction,
        args->element, args->distance, args->face, args->surfaceNormal, args->entityIdsToInclude,
        args->entityIdsToDiscard, args->visibleOnly, args->collidableOnly, args->extraInfo, args->precisionPicking);
    if (!entityID.isNull()) {
        args->entityID = entityID;
        // We recurse OctreeElements in order, so if we hit something, we can stop immediately
        keepSearching = false;
    }
    return keepSearching;
}

float findRayIntersectionSortingOp(const OctreeElementPointer& element, void* extraData) {
    RayArgs* args = static_cast<RayArgs*>(extraData);
    EntityTreeElementPointer entityTreeElementPointer = std::static_pointer_cast<EntityTreeElement>(element);
    float distance = FLT_MAX;
    // If origin is inside the cube, always check this element first
    if (entityTreeElementPointer->getAACube().contains(args->origin)) {
        distance = 0.0f;
    } else {
        float boundDistance = FLT_MAX;
        BoxFace face;
        glm::vec3 surfaceNormal;
        if (entityTreeElementPointer->getAACube().findRayIntersection(args->origin, args->direction, args->invDirection, boundDistance, face, surfaceNormal)) {
            // Don't add this cell if it's already farther than our best distance so far
            if (boundDistance < args->distance) {
                distance = boundDistance;
            }
        }
    }
    return distance;
}

EntityItemID EntityTree::findRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                                    QVector<EntityItemID> entityIdsToInclude, QVector<EntityItemID> entityIdsToDiscard,
                                    bool visibleOnly, bool collidableOnly, bool precisionPicking, 
                                    OctreeElementPointer& element, float& distance,
                                    BoxFace& face, glm::vec3& surfaceNormal, QVariantMap& extraInfo,
                                    Octree::lockType lockType, bool* accurateResult) {
    RayArgs args = { origin, direction, 1.0f / direction, entityIdsToInclude, entityIdsToDiscard,
            visibleOnly, collidableOnly, precisionPicking, element, distance, face, surfaceNormal, extraInfo, EntityItemID() };
    distance = FLT_MAX;

    bool requireLock = lockType == Octree::Lock;
    bool lockResult = withReadLock([&]{
        recurseTreeWithOperationSorted(findRayIntersectionOp, findRayIntersectionSortingOp, &args);
    }, requireLock);

    if (accurateResult) {
        *accurateResult = lockResult; // if user asked to accuracy or result, let them know this is accurate
    }

    return args.entityID;
}

bool findParabolaIntersectionOp(const OctreeElementPointer& element, void* extraData) {
    ParabolaArgs* args = static_cast<ParabolaArgs*>(extraData);
    bool keepSearching = true;
    EntityTreeElementPointer entityTreeElementPointer = std::static_pointer_cast<EntityTreeElement>(element);
    EntityItemID entityID = entityTreeElementPointer->findParabolaIntersection(args->origin, args->velocity, args->acceleration,
        args->element, args->parabolicDistance, args->face, args->surfaceNormal, args->entityIdsToInclude,
        args->entityIdsToDiscard, args->visibleOnly, args->collidableOnly, args->extraInfo, args->precisionPicking);
    if (!entityID.isNull()) {
        args->entityID = entityID;
        // We recurse OctreeElements in order, so if we hit something, we can stop immediately
        keepSearching = false;
    }
    return keepSearching;
}

float findParabolaIntersectionSortingOp(const OctreeElementPointer& element, void* extraData) {
    ParabolaArgs* args = static_cast<ParabolaArgs*>(extraData);
    EntityTreeElementPointer entityTreeElementPointer = std::static_pointer_cast<EntityTreeElement>(element);
    float distance = FLT_MAX;
    // If origin is inside the cube, always check this element first
    if (entityTreeElementPointer->getAACube().contains(args->origin)) {
        distance = 0.0f;
    } else {
        float boundDistance = FLT_MAX;
        BoxFace face;
        glm::vec3 surfaceNormal;
        if (entityTreeElementPointer->getAACube().findParabolaIntersection(args->origin, args->velocity, args->acceleration, boundDistance, face, surfaceNormal)) {
            // Don't add this cell if it's already farther than our best distance so far
            if (boundDistance < args->parabolicDistance) {
                distance = boundDistance;
            }
        }
    }
    return distance;
}

EntityItemID EntityTree::findParabolaIntersection(const PickParabola& parabola,
                                    QVector<EntityItemID> entityIdsToInclude, QVector<EntityItemID> entityIdsToDiscard,
                                    bool visibleOnly, bool collidableOnly, bool precisionPicking,
                                    OctreeElementPointer& element, glm::vec3& intersection, float& distance, float& parabolicDistance,
                                    BoxFace& face, glm::vec3& surfaceNormal, QVariantMap& extraInfo,
                                    Octree::lockType lockType, bool* accurateResult) {
    ParabolaArgs args = { parabola.origin, parabola.velocity, parabola.acceleration, entityIdsToInclude, entityIdsToDiscard,
        visibleOnly, collidableOnly, precisionPicking, element, parabolicDistance, face, surfaceNormal, extraInfo, EntityItemID() };
    parabolicDistance = FLT_MAX;
    distance = FLT_MAX;

    bool requireLock = lockType == Octree::Lock;
    bool lockResult = withReadLock([&] {
        recurseTreeWithOperationSorted(findParabolaIntersectionOp, findParabolaIntersectionSortingOp, &args);
    }, requireLock);

    if (accurateResult) {
        *accurateResult = lockResult; // if user asked to accuracy or result, let them know this is accurate
    }

    if (!args.entityID.isNull()) {
        intersection = parabola.origin + parabola.velocity * parabolicDistance + 0.5f * parabola.acceleration * parabolicDistance * parabolicDistance;
        distance = glm::distance(intersection, parabola.origin);
    }

    return args.entityID;
}


EntityItemPointer EntityTree::findClosestEntity(const glm::vec3& position, float targetRadius) {
    FindNearPointArgs args = { position, targetRadius, false, NULL, FLT_MAX };
    withReadLock([&] {
        // NOTE: This should use recursion, since this is a spatial operation
        recurseTreeWithOperation(findNearPointOperation, &args);
    });
    return args.closestEntity;
}

class FindAllNearPointArgs {
public:
    glm::vec3 position;
    float targetRadius;
    QVector<EntityItemPointer> entities;
};


bool EntityTree::findInSphereOperation(const OctreeElementPointer& element, void* extraData) {
    FindAllNearPointArgs* args = static_cast<FindAllNearPointArgs*>(extraData);
    glm::vec3 penetration;
    bool sphereIntersection = element->getAACube().findSpherePenetration(args->position, args->targetRadius, penetration);

    // If this element contains the point, then search it...
    if (sphereIntersection) {
        EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
        entityTreeElement->getEntities(args->position, args->targetRadius, args->entities);
        return true; // keep searching in case children have closer entities
    }

    // if this element doesn't contain the point, then none of it's children can contain the point, so stop searching
    return false;
}

// NOTE: assumes caller has handled locking
void EntityTree::findEntities(const glm::vec3& center, float radius, QVector<EntityItemPointer>& foundEntities) {
    FindAllNearPointArgs args = { center, radius, QVector<EntityItemPointer>() };
    // NOTE: This should use recursion, since this is a spatial operation
    recurseTreeWithOperation(findInSphereOperation, &args);

    // swap the two lists of entity pointers instead of copy
    foundEntities.swap(args.entities);
}

class FindEntitiesInCubeArgs {
public:
    FindEntitiesInCubeArgs(const AACube& cube)
        : _cube(cube), _foundEntities() {
    }

    AACube _cube;
    QVector<EntityItemPointer> _foundEntities;
};

bool EntityTree::findInCubeOperation(const OctreeElementPointer& element, void* extraData) {
    FindEntitiesInCubeArgs* args = static_cast<FindEntitiesInCubeArgs*>(extraData);
    if (element->getAACube().touches(args->_cube)) {
        EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
        entityTreeElement->getEntities(args->_cube, args->_foundEntities);
        return true;
    }
    return false;
}

// NOTE: assumes caller has handled locking
void EntityTree::findEntities(const AACube& cube, QVector<EntityItemPointer>& foundEntities) {
    FindEntitiesInCubeArgs args(cube);
    // NOTE: This should use recursion, since this is a spatial operation
    recurseTreeWithOperation(findInCubeOperation, &args);
    // swap the two lists of entity pointers instead of copy
    foundEntities.swap(args._foundEntities);
}

class FindEntitiesInBoxArgs {
public:
    FindEntitiesInBoxArgs(const AABox& box)
    : _box(box), _foundEntities() {
    }

    AABox _box;
    QVector<EntityItemPointer> _foundEntities;
};

bool EntityTree::findInBoxOperation(const OctreeElementPointer& element, void* extraData) {
    FindEntitiesInBoxArgs* args = static_cast<FindEntitiesInBoxArgs*>(extraData);
    if (element->getAACube().touches(args->_box)) {
        EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
        entityTreeElement->getEntities(args->_box, args->_foundEntities);
        return true;
    }
    return false;
}

// NOTE: assumes caller has handled locking
void EntityTree::findEntities(const AABox& box, QVector<EntityItemPointer>& foundEntities) {
    FindEntitiesInBoxArgs args(box);
    // NOTE: This should use recursion, since this is a spatial operation
    recurseTreeWithOperation(findInBoxOperation, &args);
    // swap the two lists of entity pointers instead of copy
    foundEntities.swap(args._foundEntities);
}

class FindInFrustumArgs {
public:
    ViewFrustum frustum;
    QVector<EntityItemPointer> entities;
};

bool EntityTree::findInFrustumOperation(const OctreeElementPointer& element, void* extraData) {
    FindInFrustumArgs* args = static_cast<FindInFrustumArgs*>(extraData);
    if (element->isInView(args->frustum)) {
        EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
        entityTreeElement->getEntities(args->frustum, args->entities);
        return true;
    }
    return false;
}

// NOTE: assumes caller has handled locking
void EntityTree::findEntities(const ViewFrustum& frustum, QVector<EntityItemPointer>& foundEntities) {
    FindInFrustumArgs args = { frustum, QVector<EntityItemPointer>() };
    // NOTE: This should use recursion, since this is a spatial operation
    recurseTreeWithOperation(findInFrustumOperation, &args);
    // swap the two lists of entity pointers instead of copy
    foundEntities.swap(args.entities);
}

// NOTE: assumes caller has handled locking
void EntityTree::findEntities(RecurseOctreeOperation& elementFilter,
                              QVector<EntityItemPointer>& foundEntities) {
    recurseTreeWithOperation(elementFilter, nullptr);
}

EntityItemPointer EntityTree::findEntityByID(const QUuid& id) const {
    EntityItemID entityID(id);
    return findEntityByEntityItemID(entityID);
}

EntityItemPointer EntityTree::findEntityByEntityItemID(const EntityItemID& entityID) const {
    EntityItemPointer foundEntity = nullptr;
    {
        QReadLocker locker(&_entityMapLock);
        foundEntity = _entityMap.value(entityID);
    }
    if (foundEntity && !foundEntity->getElement()) {
        // special case to maintain legacy behavior:
        // if the entity is in the map but not in the tree
        // then pretend the entity doesn't exist
        return EntityItemPointer(nullptr);
    } else {
        return foundEntity;
    }
}

void EntityTree::fixupTerseEditLogging(EntityItemProperties& properties, QList<QString>& changedProperties) {
    static quint64 lastTerseLog = 0;
    quint64 now = usecTimestampNow();

    if (now - lastTerseLog > USECS_PER_SECOND) {
        qCDebug(entities) << "-------------------------";
    }
    lastTerseLog = now;

    if (properties.simulationOwnerChanged()) {
        int simIndex = changedProperties.indexOf("simulationOwner");
        if (simIndex >= 0) {
            SimulationOwner simOwner = properties.getSimulationOwner();
            changedProperties[simIndex] = QString("simulationOwner:") + QString::number((int)simOwner.getPriority());
        }
    }

    if (properties.velocityChanged()) {
        int index = changedProperties.indexOf("velocity");
        if (index >= 0) {
            glm::vec3 value = properties.getVelocity();
            changedProperties[index] = QString("velocity:") +
                QString::number((int)value.x) + "," +
                QString::number((int)value.y) + "," +
                QString::number((int)value.z);
        }
    }

    if (properties.gravityChanged()) {
        int index = changedProperties.indexOf("gravity");
        if (index >= 0) {
            glm::vec3 value = properties.getGravity();
            QString changeHint = "0";
            if (value.x + value.y + value.z > 0) {
                changeHint = "+";
            } else if (value.x + value.y + value.z < 0) {
                changeHint = "-";
            }
            changedProperties[index] = QString("gravity:") + changeHint;
        }
    }

    if (properties.actionDataChanged()) {
        int index = changedProperties.indexOf("actionData");
        if (index >= 0) {
            QByteArray value = properties.getActionData();
            QString changeHint = serializedDynamicsToDebugString(value);
            changedProperties[index] = QString("actionData:") + changeHint;
        }
    }

    if (properties.collisionlessChanged()) {
        int index = changedProperties.indexOf("collisionless");
        if (index >= 0) {
            bool value = properties.getCollisionless();
            QString changeHint = "0";
            if (value) {
                changeHint = "1";
            }
            changedProperties[index] = QString("collisionless:") + changeHint;
        }
    }

    if (properties.dynamicChanged()) {
        int index = changedProperties.indexOf("dynamic");
        if (index >= 0) {
            bool value = properties.getDynamic();
            QString changeHint = "0";
            if (value) {
                changeHint = "1";
            }
            changedProperties[index] = QString("dynamic:") + changeHint;
        }
    }

    if (properties.lockedChanged()) {
        int index = changedProperties.indexOf("locked");
        if (index >= 0) {
            bool value = properties.getLocked();
            QString changeHint = "0";
            if (value) {
                changeHint = "1";
            }
            changedProperties[index] = QString("locked:") + changeHint;
        }
    }

    if (properties.userDataChanged()) {
        int index = changedProperties.indexOf("userData");
        if (index >= 0) {
            QString changeHint = properties.getUserData();
            changedProperties[index] = QString("userData:") + changeHint;
        }
    }

    if (properties.parentJointIndexChanged()) {
        int index = changedProperties.indexOf("parentJointIndex");
        if (index >= 0) {
            quint16 value = properties.getParentJointIndex();
            changedProperties[index] = QString("parentJointIndex:") + QString::number((int)value);
        }
    }
    if (properties.parentIDChanged()) {
        int index = changedProperties.indexOf("parentID");
        if (index >= 0) {
            QUuid value = properties.getParentID();
            changedProperties[index] = QString("parentID:") + value.toString();
        }
    }

    if (properties.jointRotationsSetChanged()) {
        int index = changedProperties.indexOf("jointRotationsSet");
        if (index >= 0) {
            auto value = properties.getJointRotationsSet().size();
            changedProperties[index] = QString("jointRotationsSet:") + QString::number((int)value);
        }
    }
    if (properties.jointRotationsChanged()) {
        int index = changedProperties.indexOf("jointRotations");
        if (index >= 0) {
            auto value = properties.getJointRotations().size();
            changedProperties[index] = QString("jointRotations:") + QString::number((int)value);
        }
    }
    if (properties.jointTranslationsSetChanged()) {
        int index = changedProperties.indexOf("jointTranslationsSet");
        if (index >= 0) {
            auto value = properties.getJointTranslationsSet().size();
            changedProperties[index] = QString("jointTranslationsSet:") + QString::number((int)value);
        }
    }
    if (properties.jointTranslationsChanged()) {
        int index = changedProperties.indexOf("jointTranslations");
        if (index >= 0) {
            auto value = properties.getJointTranslations().size();
            changedProperties[index] = QString("jointTranslations:") + QString::number((int)value);
        }
    }
    if (properties.queryAACubeChanged()) {
        int index = changedProperties.indexOf("queryAACube");
        glm::vec3 center = properties.getQueryAACube().calcCenter();
        changedProperties[index] = QString("queryAACube:") +
            QString::number((int)center.x) + "," +
            QString::number((int)center.y) + "," +
            QString::number((int)center.z) + "/" +
            QString::number(properties.getQueryAACube().getDimensions().x);
    }
    if (properties.positionChanged()) {
        int index = changedProperties.indexOf("position");
        glm::vec3 pos = properties.getPosition();
        changedProperties[index] = QString("position:") +
            QString::number((int)pos.x) + "," +
            QString::number((int)pos.y) + "," +
            QString::number((int)pos.z);
    }
    if (properties.lifetimeChanged()) {
        int index = changedProperties.indexOf("lifetime");
        if (index >= 0) {
            float value = properties.getLifetime();
            changedProperties[index] = QString("lifetime:") + QString::number((int)value);
        }
    }
}


bool EntityTree::filterProperties(EntityItemPointer& existingEntity, EntityItemProperties& propertiesIn, EntityItemProperties& propertiesOut, bool& wasChanged, FilterType filterType) {
    bool accepted = true;
    auto entityEditFilters = DependencyManager::get<EntityEditFilters>();
    if (entityEditFilters) {
        auto position = existingEntity ? existingEntity->getWorldPosition() : propertiesIn.getPosition();
        auto entityID = existingEntity ? existingEntity->getEntityItemID() : EntityItemID();
        accepted = entityEditFilters->filter(position, propertiesIn, propertiesOut, wasChanged, filterType, entityID, existingEntity);
    }

    return accepted;
}

void EntityTree::bumpTimestamp(EntityItemProperties& properties) { //fixme put class/header
    const quint64 LAST_EDITED_SERVERSIDE_BUMP = 1; // usec
    // also bump up the lastEdited time of the properties so that the interface that created this edit
    // will accept our adjustment to lifetime back into its own entity-tree.
    if (properties.getLastEdited() == UNKNOWN_CREATED_TIME) {
        properties.setLastEdited(usecTimestampNow());
    }
    properties.setLastEdited(properties.getLastEdited() + LAST_EDITED_SERVERSIDE_BUMP);
}

bool EntityTree::isScriptInWhitelist(const QString& scriptProperty) {

    // grab a URL representation of the entity script so we can check the host for this script
    auto entityScriptURL = QUrl::fromUserInput(scriptProperty);

    for (const auto& whiteListedPrefix : _entityScriptSourceWhitelist) {
        auto whiteListURL = QUrl::fromUserInput(whiteListedPrefix);

        // check if this script URL matches the whitelist domain and, optionally, is beneath the path
        if (entityScriptURL.host().compare(whiteListURL.host(), Qt::CaseInsensitive) == 0 &&
            entityScriptURL.path().startsWith(whiteListURL.path(), Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

void EntityTree::startChallengeOwnershipTimer(const EntityItemID& entityItemID) {
    QTimer* _challengeOwnershipTimeoutTimer = new QTimer(this);
    connect(this, &EntityTree::killChallengeOwnershipTimeoutTimer, this, [=](const QString& certID) {
        QReadLocker locker(&_entityCertificateIDMapLock);
        EntityItemID id = _entityCertificateIDMap.value(certID);
        if (entityItemID == id && _challengeOwnershipTimeoutTimer) {
            _challengeOwnershipTimeoutTimer->stop();
            _challengeOwnershipTimeoutTimer->deleteLater();
        }
    });
    connect(_challengeOwnershipTimeoutTimer, &QTimer::timeout, this, [=]() {
        qCDebug(entities) << "Ownership challenge timed out, deleting entity" << entityItemID;
        withWriteLock([&] {
            deleteEntity(entityItemID, true);
        });
        if (_challengeOwnershipTimeoutTimer) {
            _challengeOwnershipTimeoutTimer->stop();
            _challengeOwnershipTimeoutTimer->deleteLater();
        }
    });
    _challengeOwnershipTimeoutTimer->setSingleShot(true);
    _challengeOwnershipTimeoutTimer->start(5000);
}

QByteArray EntityTree::computeNonce(const QString& certID, const QString ownerKey) {
    QUuid nonce = QUuid::createUuid();  //random, 5-hex value, separated by "-"
    QByteArray nonceBytes = nonce.toByteArray();

    QWriteLocker locker(&_certNonceMapLock);
    _certNonceMap.insert(certID, QPair<QUuid, QString>(nonce, ownerKey));

    return nonceBytes;
}

bool EntityTree::verifyNonce(const QString& certID, const QString& nonce, EntityItemID& id) {
    {
        QReadLocker certIdMapLocker(&_entityCertificateIDMapLock);
        id = _entityCertificateIDMap.value(certID);
    }

    QString actualNonce, key;
    {
        QWriteLocker locker(&_certNonceMapLock);
        QPair<QUuid, QString> sent = _certNonceMap.take(certID);
        actualNonce = sent.first.toString();
        key = sent.second;
    }

    QString annotatedKey = "-----BEGIN PUBLIC KEY-----\n" + key.insert(64, "\n") + "\n-----END PUBLIC KEY-----\n"; 
    QByteArray hashedActualNonce = QCryptographicHash::hash(QByteArray(actualNonce.toUtf8()), QCryptographicHash::Sha256);
    bool verificationSuccess = EntityItemProperties::verifySignature(annotatedKey.toUtf8(), hashedActualNonce, QByteArray::fromBase64(nonce.toUtf8()));

    if (verificationSuccess) {
        qCDebug(entities) << "Ownership challenge for Cert ID" << certID << "succeeded.";
    } else {
        qCDebug(entities) << "Ownership challenge for Cert ID" << certID << "failed. Actual nonce:" << actualNonce <<
            "\nHashed actual nonce (digest):" << hashedActualNonce << "\nSent nonce (signature)" << nonce << "\nKey" << key;
    }

    return verificationSuccess;
}

void EntityTree::processChallengeOwnershipRequestPacket(ReceivedMessage& message, const SharedNodePointer& sourceNode) {
    int certIDByteArraySize;
    int textByteArraySize;
    int nodeToChallengeByteArraySize;

    message.readPrimitive(&certIDByteArraySize);
    message.readPrimitive(&textByteArraySize);
    message.readPrimitive(&nodeToChallengeByteArraySize);

    QByteArray certID(message.read(certIDByteArraySize));
    QByteArray text(message.read(textByteArraySize));
    QByteArray nodeToChallenge(message.read(nodeToChallengeByteArraySize));

    sendChallengeOwnershipRequestPacket(certID, text, nodeToChallenge, sourceNode);
}

void EntityTree::processChallengeOwnershipReplyPacket(ReceivedMessage& message, const SharedNodePointer& sourceNode) {
    auto nodeList = DependencyManager::get<NodeList>();

    int certIDByteArraySize;
    int textByteArraySize;
    int challengingNodeUUIDByteArraySize;

    message.readPrimitive(&certIDByteArraySize);
    message.readPrimitive(&textByteArraySize);
    message.readPrimitive(&challengingNodeUUIDByteArraySize);

    QByteArray certID(message.read(certIDByteArraySize));
    QByteArray text(message.read(textByteArraySize));
    QUuid challengingNode = QUuid::fromRfc4122(message.read(challengingNodeUUIDByteArraySize));

    auto challengeOwnershipReplyPacket = NLPacket::create(PacketType::ChallengeOwnershipReply,
        certIDByteArraySize + text.length() + 2 * sizeof(int),
        true);
    challengeOwnershipReplyPacket->writePrimitive(certIDByteArraySize);
    challengeOwnershipReplyPacket->writePrimitive(text.length());
    challengeOwnershipReplyPacket->write(certID);
    challengeOwnershipReplyPacket->write(text);

    nodeList->sendPacket(std::move(challengeOwnershipReplyPacket), *(nodeList->nodeWithUUID(challengingNode)));
}

void EntityTree::sendChallengeOwnershipPacket(const QString& certID, const QString& ownerKey, const EntityItemID& entityItemID, const SharedNodePointer& senderNode) {
    // 1. Obtain a nonce
    auto nodeList = DependencyManager::get<NodeList>();

    QByteArray text = computeNonce(certID, ownerKey);

    if (text == "") {
        qCDebug(entities) << "CRITICAL ERROR: Couldn't compute nonce. Deleting entity...";
        withWriteLock([&] {
            deleteEntity(entityItemID, true);
        });
    } else {
        qCDebug(entities) << "Challenging ownership of Cert ID" << certID;
        // 2. Send the nonce to the rezzing avatar's node
        QByteArray certIDByteArray = certID.toUtf8();
        int certIDByteArraySize = certIDByteArray.size();
        auto challengeOwnershipPacket = NLPacket::create(PacketType::ChallengeOwnership,
            certIDByteArraySize + text.length() + 2 * sizeof(int),
            true);
        challengeOwnershipPacket->writePrimitive(certIDByteArraySize);
        challengeOwnershipPacket->writePrimitive(text.length());
        challengeOwnershipPacket->write(certIDByteArray);
        challengeOwnershipPacket->write(text);
        nodeList->sendPacket(std::move(challengeOwnershipPacket), *senderNode);

        // 3. Kickoff a 10-second timeout timer that deletes the entity if we don't get an ownership response in time
        if (thread() != QThread::currentThread()) {
            QMetaObject::invokeMethod(this, "startChallengeOwnershipTimer", Q_ARG(const EntityItemID&, entityItemID));
            return;
        } else {
            startChallengeOwnershipTimer(entityItemID);
        }
    }
}

void EntityTree::sendChallengeOwnershipRequestPacket(const QByteArray& certID, const QByteArray& text, const QByteArray& nodeToChallenge, const SharedNodePointer& senderNode) {
    auto nodeList = DependencyManager::get<NodeList>();

    // In this case, Client A is challenging Client B. Client A is inspecting a certified entity that it wants
    //     to make sure belongs to Avatar B.
    QByteArray senderNodeUUID = senderNode->getUUID().toRfc4122();

    int certIDByteArraySize = certID.length();
    int TextByteArraySize = text.length();
    int senderNodeUUIDSize = senderNodeUUID.length();

    auto challengeOwnershipPacket = NLPacket::create(PacketType::ChallengeOwnershipRequest,
        certIDByteArraySize + TextByteArraySize + senderNodeUUIDSize + 3 * sizeof(int),
        true);
    challengeOwnershipPacket->writePrimitive(certIDByteArraySize);
    challengeOwnershipPacket->writePrimitive(TextByteArraySize);
    challengeOwnershipPacket->writePrimitive(senderNodeUUIDSize);
    challengeOwnershipPacket->write(certID);
    challengeOwnershipPacket->write(text);
    challengeOwnershipPacket->write(senderNodeUUID);

    nodeList->sendPacket(std::move(challengeOwnershipPacket), *(nodeList->nodeWithUUID(QUuid::fromRfc4122(nodeToChallenge))));
}

void EntityTree::validatePop(const QString& certID, const EntityItemID& entityItemID, const SharedNodePointer& senderNode) {
    // Start owner verification.
    auto nodeList = DependencyManager::get<NodeList>();
    //     First, asynchronously hit "proof_of_purchase_status?transaction_type=transfer" endpoint.
    QNetworkAccessManager& networkAccessManager = NetworkAccessManager::getInstance();
    QNetworkRequest networkRequest;
    networkRequest.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QUrl requestURL = NetworkingConstants::METAVERSE_SERVER_URL();
    requestURL.setPath("/api/v1/commerce/proof_of_purchase_status/transfer");
    QJsonObject request;
    request["certificate_id"] = certID;
    networkRequest.setUrl(requestURL);

    QNetworkReply* networkReply = networkAccessManager.put(networkRequest, QJsonDocument(request).toJson());

    connect(networkReply, &QNetworkReply::finished, [this, networkReply, entityItemID, certID, senderNode]() {
        QJsonObject jsonObject = QJsonDocument::fromJson(networkReply->readAll()).object();
        jsonObject = jsonObject["data"].toObject();

        if (networkReply->error() == QNetworkReply::NoError) {
            if (!jsonObject["invalid_reason"].toString().isEmpty()) {
                qCDebug(entities) << "invalid_reason not empty, deleting entity" << entityItemID;
                withWriteLock([&] {
                    deleteEntity(entityItemID, true);
                });
            } else if (jsonObject["transfer_status"].toArray().first().toString() == "failed") {
                qCDebug(entities) << "'transfer_status' is 'failed', deleting entity" << entityItemID;
                withWriteLock([&] {
                    deleteEntity(entityItemID, true);
                });
            } else {
                // Second, challenge ownership of the PoP cert
                // (ignore pending status; a failure will be cleaned up during DDV)
                sendChallengeOwnershipPacket(certID,
                    jsonObject["transfer_recipient_key"].toString(),
                    entityItemID,
                    senderNode);
            }
        } else {
            qCDebug(entities) << "Call to" << networkReply->url() << "failed with error" << networkReply->error() << "; deleting entity" << entityItemID
                << "More info:" << jsonObject;
            withWriteLock([&] {
                deleteEntity(entityItemID, true);
            });
        }

        networkReply->deleteLater();
    });
}

void EntityTree::processChallengeOwnershipPacket(ReceivedMessage& message, const SharedNodePointer& sourceNode) {
    int certIDByteArraySize;
    int textByteArraySize;

    message.readPrimitive(&certIDByteArraySize);
    message.readPrimitive(&textByteArraySize);

    QString certID(message.read(certIDByteArraySize));
    QString text(message.read(textByteArraySize));

    emit killChallengeOwnershipTimeoutTimer(certID);

    EntityItemID id;
    if (!verifyNonce(certID, text, id)) {
        if (!id.isNull()) {
            deleteEntity(id, true);
        }
    }
}

int EntityTree::processEditPacketData(ReceivedMessage& message, const unsigned char* editData, int maxLength,
                                     const SharedNodePointer& senderNode) {

    if (!getIsServer()) {
        qCWarning(entities) << "EntityTree::processEditPacketData() should only be called on a server tree.";
        return 0;
    }

    int processedBytes = 0;
    bool isAdd = false;
    bool isClone = false;
    // we handle these types of "edit" packets
    switch (message.getType()) {
        case PacketType::EntityErase: {
            QByteArray dataByteArray = QByteArray::fromRawData(reinterpret_cast<const char*>(editData), maxLength);
            processedBytes = processEraseMessageDetails(dataByteArray, senderNode);
            break;
        }

        case PacketType::EntityClone:
            isClone = true; // fall through to next case
            // FALLTHRU
        case PacketType::EntityAdd:
            isAdd = true;  // fall through to next case
            // FALLTHRU
        case PacketType::EntityPhysics:
        case PacketType::EntityEdit: {
            quint64 startDecode = 0, endDecode = 0;
            quint64 startLookup = 0, endLookup = 0;
            quint64 startUpdate = 0, endUpdate = 0;
            quint64 startCreate = 0, endCreate = 0;
            quint64 startFilter = 0, endFilter = 0;
            quint64 startLogging = 0, endLogging = 0;

            bool suppressDisallowedClientScript = false;
            bool suppressDisallowedServerScript = false;
            bool isPhysics = message.getType() == PacketType::EntityPhysics;

            _totalEditMessages++;

            EntityItemID entityItemID;
            EntityItemProperties properties;
            startDecode = usecTimestampNow();

            bool validEditPacket = false;
            EntityItemID entityIDToClone;
            EntityItemPointer entityToClone;
            if (isClone) {
                QByteArray buffer = QByteArray::fromRawData(reinterpret_cast<const char*>(editData), maxLength);
                validEditPacket = EntityItemProperties::decodeCloneEntityMessage(buffer, processedBytes, entityIDToClone, entityItemID);
                if (validEditPacket) {
                    entityToClone = findEntityByEntityItemID(entityIDToClone);
                    if (entityToClone) {
                        properties = entityToClone->getProperties();
                    }
                }
            } else {
                validEditPacket = EntityItemProperties::decodeEntityEditPacket(editData, maxLength, processedBytes, entityItemID, properties);
            }

            endDecode = usecTimestampNow();

            EntityItemPointer existingEntity;
            if (!isAdd) {
                // search for the entity by EntityItemID
                startLookup = usecTimestampNow();
                existingEntity = findEntityByEntityItemID(entityItemID);
                endLookup = usecTimestampNow();
                if (!existingEntity) {
                    // this is not an add-entity operation, and we don't know about the identified entity.
                    validEditPacket = false;
                }
            }

            if (validEditPacket && !_entityScriptSourceWhitelist.isEmpty()) {

                bool wasDeletedBecauseOfClientScript = false;

                // check the client entity script to make sure its URL is in the whitelist
                if (!properties.getScript().isEmpty()) {
                    bool clientScriptPassedWhitelist = isScriptInWhitelist(properties.getScript());

                    if (!clientScriptPassedWhitelist) {
                        if (wantEditLogging()) {
                            qCDebug(entities) << "User [" << senderNode->getUUID()
                                << "] attempting to set entity script not on whitelist, edit rejected";
                        }

                        // If this was an add, we also want to tell the client that sent this edit that the entity was not added.
                        if (isAdd) {
                            QWriteLocker locker(&_recentlyDeletedEntitiesLock);
                            _recentlyDeletedEntityItemIDs.insert(usecTimestampNow(), entityItemID);
                            validEditPacket = false;
                            wasDeletedBecauseOfClientScript = true;
                        } else {
                            suppressDisallowedClientScript = true;
                        }
                    }
                }

                // check all server entity scripts to make sure their URLs are in the whitelist
                if (!properties.getServerScripts().isEmpty()) {
                    bool serverScriptPassedWhitelist = isScriptInWhitelist(properties.getServerScripts());

                    if (!serverScriptPassedWhitelist) {
                        if (wantEditLogging()) {
                            qCDebug(entities) << "User [" << senderNode->getUUID()
                                << "] attempting to set server entity script not on whitelist, edit rejected";
                        }

                        // If this was an add, we also want to tell the client that sent this edit that the entity was not added.
                        if (isAdd) {
                            // Make sure we didn't already need to send back a delete because the client script failed
                            // the whitelist check
                            if (!wasDeletedBecauseOfClientScript) {
                                QWriteLocker locker(&_recentlyDeletedEntitiesLock);
                                _recentlyDeletedEntityItemIDs.insert(usecTimestampNow(), entityItemID);
                                validEditPacket = false;
                            }
                        } else {
                            suppressDisallowedServerScript = true;
                        }
                    }
                }

            }

            if (!isClone) {
                if ((isAdd || properties.lifetimeChanged()) &&
                    ((!senderNode->getCanRez() && senderNode->getCanRezTmp()) ||
                    (!senderNode->getCanRezCertified() && senderNode->getCanRezTmpCertified()))) {
                    // this node is only allowed to rez temporary entities.  if need be, cap the lifetime.
                    if (properties.getLifetime() == ENTITY_ITEM_IMMORTAL_LIFETIME ||
                        properties.getLifetime() > _maxTmpEntityLifetime) {
                        properties.setLifetime(_maxTmpEntityLifetime);
                        bumpTimestamp(properties);
                    }
                }

                if (isAdd && properties.getLocked() && !senderNode->isAllowedEditor()) {
                    // if a node can't change locks, don't allow it to create an already-locked entity -- automatically
                    // clear the locked property and allow the unlocked entity to be created.
                    properties.setLocked(false);
                    bumpTimestamp(properties);
                }
            }

            // If we got a valid edit packet, then it could be a new entity or it could be an update to
            // an existing entity... handle appropriately
            if (validEditPacket) {
                startFilter = usecTimestampNow();
                bool wasChanged = false;
                // Having (un)lock rights bypasses the filter, unless it's a physics result.
                FilterType filterType = isPhysics ? FilterType::Physics : (isAdd ? FilterType::Add : FilterType::Edit);
                bool allowed = (!isPhysics && senderNode->isAllowedEditor()) || filterProperties(existingEntity, properties, properties, wasChanged, filterType);
                if (!allowed) {
                    auto timestamp = properties.getLastEdited();
                    properties = EntityItemProperties();
                    properties.setLastEdited(timestamp);
                }
                if (!allowed || wasChanged) {
                    bumpTimestamp(properties);
                    // For now, free ownership on any modification.
                    properties.clearSimulationOwner();
                }
                endFilter = usecTimestampNow();

                if (existingEntity && !isAdd) {

                    if (suppressDisallowedClientScript) {
                        bumpTimestamp(properties);
                        properties.setScript(existingEntity->getScript());
                    }

                    if (suppressDisallowedServerScript) {
                        bumpTimestamp(properties);
                        properties.setServerScripts(existingEntity->getServerScripts());
                    }

                    // if the EntityItem exists, then update it
                    startLogging = usecTimestampNow();
                    if (wantEditLogging()) {
                        qCDebug(entities) << "User [" << senderNode->getUUID() << "] editing entity. ID:" << entityItemID;
                        qCDebug(entities) << "   properties:" << properties;
                    }
                    if (wantTerseEditLogging()) {
                        QList<QString> changedProperties = properties.listChangedProperties();
                        fixupTerseEditLogging(properties, changedProperties);
                        qCDebug(entities) << senderNode->getUUID() << "edit" <<
                            existingEntity->getDebugName() << changedProperties;
                    }
                    endLogging = usecTimestampNow();

                    startUpdate = usecTimestampNow();
                    if (!isPhysics) {
                        properties.setLastEditedBy(senderNode->getUUID());
                    }
                    updateEntity(existingEntity, properties, senderNode);
                    existingEntity->markAsChangedOnServer();
                    endUpdate = usecTimestampNow();
                    _totalUpdates++;
                } else if (isAdd) {
                    bool failedAdd = !allowed;
                    bool isCertified = !properties.getCertificateID().isEmpty();
                    bool isCloneable = properties.getCloneable();
                    int cloneLimit = properties.getCloneLimit();
                    if (!allowed) {
                        qCDebug(entities) << "Filtered entity add. ID:" << entityItemID;
                    } else if (!isClone && !isCertified && !senderNode->getCanRez() && !senderNode->getCanRezTmp()) {
                        failedAdd = true;
                        qCDebug(entities) << "User without 'uncertified rez rights' [" << senderNode->getUUID()
                            << "] attempted to add an uncertified entity with ID:" << entityItemID;
                    } else if (!isClone && isCertified && !senderNode->getCanRezCertified() && !senderNode->getCanRezTmpCertified()) {
                        failedAdd = true;
                        qCDebug(entities) << "User without 'certified rez rights' [" << senderNode->getUUID()
                            << "] attempted to add a certified entity with ID:" << entityItemID;
                    } else if (isClone && isCertified) {
                        failedAdd = true;
                        qCDebug(entities) << "User attempted to clone certified entity from entity ID:" << entityIDToClone;
                    } else if (isClone && !isCloneable) {
                        failedAdd = true;
                        qCDebug(entities) << "User attempted to clone non-cloneable entity from entity ID:" << entityIDToClone;
                    } else if (isClone && entityToClone && entityToClone->getCloneIDs().size() >= cloneLimit && cloneLimit != 0) {
                        failedAdd = true;
                        qCDebug(entities) << "User attempted to clone entity ID:" << entityIDToClone << " which reached it's cloneable limit.";
                    } else {
                        if (isClone) {
                            properties.convertToCloneProperties(entityIDToClone);
                        }

                        // this is a new entity... assign a new entityID
                        properties.setCreated(properties.getLastEdited());
                        properties.setLastEditedBy(senderNode->getUUID());
                        startCreate = usecTimestampNow();
                        EntityItemPointer newEntity = addEntity(entityItemID, properties);
                        endCreate = usecTimestampNow();
                        _totalCreates++;

                        if (newEntity && isCertified && getIsServer()) {
                            if (!properties.verifyStaticCertificateProperties()) {
                                qCDebug(entities) << "User" << senderNode->getUUID()
                                    << "attempted to add a certified entity with ID" << entityItemID << "which failed"
                                    << "static certificate verification.";
                                // Delete the entity we just added if it doesn't pass static certificate verification
                                deleteEntity(entityItemID, true);
                            } else {
                                validatePop(properties.getCertificateID(), entityItemID, senderNode);
                            }
                        }

                        if (newEntity && isClone) {
                            entityToClone->addCloneID(newEntity->getEntityItemID());
                            newEntity->setCloneOriginID(entityIDToClone);
                        }

                        if (newEntity) {
                            newEntity->markAsChangedOnServer();
                            notifyNewlyCreatedEntity(*newEntity, senderNode);
                            
                            startLogging = usecTimestampNow();
                            if (wantEditLogging()) {
                                qCDebug(entities) << "User [" << senderNode->getUUID() << "] added entity. ID:"
                                                  << newEntity->getEntityItemID();
                                qCDebug(entities) << "   properties:" << properties;
                            }
                            if (wantTerseEditLogging()) {
                                QList<QString> changedProperties = properties.listChangedProperties();
                                fixupTerseEditLogging(properties, changedProperties);
                                qCDebug(entities) << senderNode->getUUID() << "add" << entityItemID << changedProperties;
                            }
                            endLogging = usecTimestampNow();

                        } else {
                            failedAdd = true;
                            qCDebug(entities) << "Add entity failed ID:" << entityItemID;
                        }
                    }
                    if (failedAdd) { // Let client know it failed, so that they don't have an entity that no one else sees.
                        QWriteLocker locker(&_recentlyDeletedEntitiesLock);
                        _recentlyDeletedEntityItemIDs.insert(usecTimestampNow(), entityItemID);
                    }
                } else {
                    HIFI_FCDEBUG(entities(), "Edit failed. [" << message.getType() <<"] " <<
                            "entity id:" << entityItemID << 
                            "existingEntity pointer:" << existingEntity.get());
                }
            }


            _totalDecodeTime += endDecode - startDecode;
            _totalLookupTime += endLookup - startLookup;
            _totalUpdateTime += endUpdate - startUpdate;
            _totalCreateTime += endCreate - startCreate;
            _totalLoggingTime += endLogging - startLogging;
            _totalFilterTime += endFilter - startFilter;

            break;
        }

        default:
            processedBytes = 0;
            break;
    }
    return processedBytes;
}


void EntityTree::notifyNewlyCreatedEntity(const EntityItem& newEntity, const SharedNodePointer& senderNode) {
    _newlyCreatedHooksLock.lockForRead();
    for (int i = 0; i < _newlyCreatedHooks.size(); i++) {
        _newlyCreatedHooks[i]->entityCreated(newEntity, senderNode);
    }
    _newlyCreatedHooksLock.unlock();
}

void EntityTree::addNewlyCreatedHook(NewlyCreatedEntityHook* hook) {
    _newlyCreatedHooksLock.lockForWrite();
    _newlyCreatedHooks.push_back(hook);
    _newlyCreatedHooksLock.unlock();
}

void EntityTree::removeNewlyCreatedHook(NewlyCreatedEntityHook* hook) {
    _newlyCreatedHooksLock.lockForWrite();
    for (int i = 0; i < _newlyCreatedHooks.size(); i++) {
        if (_newlyCreatedHooks[i] == hook) {
            _newlyCreatedHooks.erase(_newlyCreatedHooks.begin() + i);
            break;
        }
    }
    _newlyCreatedHooksLock.unlock();
}


void EntityTree::releaseSceneEncodeData(OctreeElementExtraEncodeData* extraEncodeData) const {
    extraEncodeData->clear();
}

void EntityTree::entityChanged(EntityItemPointer entity) {
    if (entity->isSimulated()) {
        _simulation->changeEntity(entity);
    }
}

void EntityTree::fixupNeedsParentFixups() {
    PROFILE_RANGE(simulation_physics, "FixupParents");
    MovingEntitiesOperator moveOperator;
    QVector<EntityItemWeakPointer> entitiesToFixup;
    {
        QWriteLocker locker(&_needsParentFixupLock);
        entitiesToFixup = _needsParentFixup;
        _needsParentFixup.clear();
    }

    QMutableVectorIterator<EntityItemWeakPointer> iter(entitiesToFixup);
    while (iter.hasNext()) {
        EntityItemWeakPointer entityWP = iter.next();
        EntityItemPointer entity = entityWP.lock();
        if (!entity) {
            // entity was deleted before we found its parent
            iter.remove();
            continue;
        }

        entity->requiresRecalcBoxes();
        bool queryAACubeSuccess { false };
        bool maxAACubeSuccess { false };
        AACube newCube = entity->getQueryAACube(queryAACubeSuccess);
        if (queryAACubeSuccess) {
            // make sure queryAACube encompasses maxAACube
            AACube maxAACube = entity->getMaximumAACube(maxAACubeSuccess);
            if (maxAACubeSuccess && !newCube.contains(maxAACube)) {
                newCube = maxAACube;
            }
        }

        bool doMove = false;
        if (entity->isParentIDValid() && maxAACubeSuccess) { // maxAACubeSuccess of true means all ancestors are known
            iter.remove(); // this entity is all hooked up; we can remove it from the list
            // this entity's parent was previously not known, and now is.  Update its location in the EntityTree...
            doMove = true;
            // the bounds on the render-item may need to be updated, the rigid body in the physics engine may
            // need to be moved.
            entity->markDirtyFlags(Simulation::DIRTY_MOTION_TYPE |
                                   Simulation::DIRTY_COLLISION_GROUP |
                                   Simulation::DIRTY_TRANSFORM);
            entityChanged(entity);
            entity->forEachDescendant([&](SpatiallyNestablePointer object) {
                if (object->getNestableType() == NestableType::Entity) {
                    EntityItemPointer descendantEntity = std::static_pointer_cast<EntityItem>(object);
                    descendantEntity->markDirtyFlags(Simulation::DIRTY_MOTION_TYPE |
                                                     Simulation::DIRTY_COLLISION_GROUP |
                                                     Simulation::DIRTY_TRANSFORM);
                    entityChanged(descendantEntity);
                }
            });
            entity->locationChanged(true);

            // Update our parent's bounding box
            bool success = false;
            auto parent = entity->getParentPointer(success);
            if (success && parent) {
                parent->updateQueryAACube();
            }

            entity->postParentFixup();
        } else if (getIsServer() || _avatarIDs.contains(entity->getParentID())) {
            // this is a child of an avatar, which the entity server will never have
            // a SpatiallyNestable object for.  Add it to a list for cleanup when the avatar leaves.
            if (!_childrenOfAvatars.contains(entity->getParentID())) {
                _childrenOfAvatars[entity->getParentID()] = QSet<EntityItemID>();
            }
            _childrenOfAvatars[entity->getParentID()] += entity->getEntityItemID();
            doMove = true;
            iter.remove(); // and pull it out of the list
        }

        if (queryAACubeSuccess && doMove) {
            moveOperator.addEntityToMoveList(entity, newCube);
        }
    }

    if (moveOperator.hasMovingEntities()) {
        PerformanceTimer perfTimer("recurseTreeWithOperator");
        recurseTreeWithOperator(&moveOperator);
    }

    {
        QWriteLocker locker(&_needsParentFixupLock);
        // add back the entities that did not get fixup
        _needsParentFixup.append(entitiesToFixup);
    }
}

void EntityTree::deleteDescendantsOfAvatar(QUuid avatarID) {
    if (_childrenOfAvatars.contains(avatarID)) {
        deleteEntities(_childrenOfAvatars[avatarID]);
        _childrenOfAvatars.remove(avatarID);
    }
}

void EntityTree::removeFromChildrenOfAvatars(EntityItemPointer entity) {
    QUuid avatarID = entity->getParentID();
    if (_childrenOfAvatars.contains(avatarID)) {
        _childrenOfAvatars[avatarID].remove(entity->getID());
    }
}

void EntityTree::addToNeedsParentFixupList(EntityItemPointer entity) {
    QWriteLocker locker(&_needsParentFixupLock);
    _needsParentFixup.append(entity);
}

void EntityTree::update(bool simulate) {
    PROFILE_RANGE(simulation_physics, "UpdateTree");
    PerformanceTimer perfTimer("updateTree");
    withWriteLock([&] {
        fixupNeedsParentFixups();
        if (simulate && _simulation) {
            _simulation->updateEntities();
            {
                PROFILE_RANGE(simulation_physics, "Deletes");
                SetOfEntities deadEntities;
                _simulation->takeDeadEntities(deadEntities);
                if (!deadEntities.empty()) {
                    // translate into list of ID's
                    QSet<EntityItemID> idsToDelete;

                    for (auto entity : deadEntities) {
                        idsToDelete.insert(entity->getEntityItemID());
                    }

                    // delete these things the roundabout way
                    deleteEntities(idsToDelete, true);
                }
            }
        }
    });
}

quint64 EntityTree::getAdjustedConsiderSince(quint64 sinceTime) {
    return (sinceTime - DELETED_ENTITIES_EXTRA_USECS_TO_CONSIDER);
}


bool EntityTree::hasEntitiesDeletedSince(quint64 sinceTime) {
    quint64 considerEntitiesSince = getAdjustedConsiderSince(sinceTime);

    // we can probably leverage the ordered nature of QMultiMap to do this quickly...
    bool hasSomethingNewer = false;

    QReadLocker locker(&_recentlyDeletedEntitiesLock);
    QMultiMap<quint64, QUuid>::const_iterator iterator = _recentlyDeletedEntityItemIDs.constBegin();
    while (iterator != _recentlyDeletedEntityItemIDs.constEnd()) {
        if (iterator.key() > considerEntitiesSince) {
            hasSomethingNewer = true;
            break; // if we have at least one item, we don't need to keep searching
        }
        ++iterator;
    }

#ifdef EXTRA_ERASE_DEBUGGING
    if (hasSomethingNewer) {
        int elapsed = usecTimestampNow() - considerEntitiesSince;
        int difference = considerEntitiesSince - sinceTime;
        qCDebug(entities) << "EntityTree::hasEntitiesDeletedSince() sinceTime:" << sinceTime 
                    << "considerEntitiesSince:" << considerEntitiesSince << "elapsed:" << elapsed << "difference:" << difference;
    }
#endif

    return hasSomethingNewer;
}

// called by the server when it knows all nodes have been sent deleted packets
void EntityTree::forgetEntitiesDeletedBefore(quint64 sinceTime) {
    quint64 considerSinceTime = sinceTime - DELETED_ENTITIES_EXTRA_USECS_TO_CONSIDER;
    QSet<quint64> keysToRemove;
    QWriteLocker locker(&_recentlyDeletedEntitiesLock);
    QMultiMap<quint64, QUuid>::iterator iterator = _recentlyDeletedEntityItemIDs.begin();

    // First find all the keys in the map that are older and need to be deleted
    while (iterator != _recentlyDeletedEntityItemIDs.end()) {
        if (iterator.key() <= considerSinceTime) {
            keysToRemove << iterator.key();
        }
        ++iterator;
    }

    // Now run through the keysToRemove and remove them
    foreach (quint64 value, keysToRemove) {
        _recentlyDeletedEntityItemIDs.remove(value);
    }
}


bool EntityTree::shouldEraseEntity(EntityItemID entityID, const SharedNodePointer& sourceNode) {
    EntityItemPointer existingEntity;

    auto startLookup = usecTimestampNow();
    existingEntity = findEntityByEntityItemID(entityID);
    auto endLookup = usecTimestampNow();
    _totalLookupTime += endLookup - startLookup;

    auto startFilter = usecTimestampNow();
    FilterType filterType = FilterType::Delete;
    EntityItemProperties dummyProperties;
    bool wasChanged = false;

    bool allowed = (sourceNode->isAllowedEditor()) || filterProperties(existingEntity, dummyProperties, dummyProperties, wasChanged, filterType);
    auto endFilter = usecTimestampNow();

    _totalFilterTime += endFilter - startFilter;

    if (allowed) {
        if (wantEditLogging() || wantTerseEditLogging()) {
            qCDebug(entities) << "User [" << sourceNode->getUUID() << "] deleting entity. ID:" << entityID;
        }
    } else if (wantEditLogging() || wantTerseEditLogging()) {
        qCDebug(entities) << "User [" << sourceNode->getUUID() << "] attempted to deleteentity. ID:" << entityID << " Filter rejected erase.";
    }

    return allowed;
}


// TODO: consider consolidating processEraseMessageDetails() and processEraseMessage()
int EntityTree::processEraseMessage(ReceivedMessage& message, const SharedNodePointer& sourceNode) {
    #ifdef EXTRA_ERASE_DEBUGGING
        qCDebug(entities) << "EntityTree::processEraseMessage()";
    #endif
    withWriteLock([&] {
        message.seek(sizeof(OCTREE_PACKET_FLAGS) + sizeof(OCTREE_PACKET_SEQUENCE) + sizeof(OCTREE_PACKET_SENT_TIME));

        uint16_t numberOfIDs = 0; // placeholder for now
        message.readPrimitive(&numberOfIDs);

        if (numberOfIDs > 0) {
            QSet<EntityItemID> entityItemIDsToDelete;

            for (size_t i = 0; i < numberOfIDs; i++) {

                if (NUM_BYTES_RFC4122_UUID > message.getBytesLeftToRead()) {
                    qCDebug(entities) << "EntityTree::processEraseMessage().... bailing because not enough bytes in buffer";
                    break; // bail to prevent buffer overflow
                }

                QUuid entityID = QUuid::fromRfc4122(message.readWithoutCopy(NUM_BYTES_RFC4122_UUID));
                #ifdef EXTRA_ERASE_DEBUGGING
                    qCDebug(entities) << "    ---- EntityTree::processEraseMessage() contained ID:" << entityID;
                #endif

                EntityItemID entityItemID(entityID);

                if (shouldEraseEntity(entityID, sourceNode)) {
                    entityItemIDsToDelete << entityItemID;
                    cleanupCloneIDs(entityItemID);
                }
            }
            deleteEntities(entityItemIDsToDelete, true, true);
        }
    });
    return message.getPosition();
}

// This version skips over the header
// NOTE: Caller must lock the tree before calling this.
// TODO: consider consolidating processEraseMessageDetails() and processEraseMessage()
int EntityTree::processEraseMessageDetails(const QByteArray& dataByteArray, const SharedNodePointer& sourceNode) {
    #ifdef EXTRA_ERASE_DEBUGGING
        qCDebug(entities) << "EntityTree::processEraseMessageDetails()";
    #endif
    const unsigned char* packetData = (const unsigned char*)dataByteArray.constData();
    const unsigned char* dataAt = packetData;
    size_t packetLength = dataByteArray.size();
    size_t processedBytes = 0;

    uint16_t numberOfIds = 0; // placeholder for now
    memcpy(&numberOfIds, dataAt, sizeof(numberOfIds));
    dataAt += sizeof(numberOfIds);
    processedBytes += sizeof(numberOfIds);

    if (numberOfIds > 0) {
        QSet<EntityItemID> entityItemIDsToDelete;

        for (size_t i = 0; i < numberOfIds; i++) {


            if (processedBytes + NUM_BYTES_RFC4122_UUID > packetLength) {
                qCDebug(entities) << "EntityTree::processEraseMessageDetails().... bailing because not enough bytes in buffer";
                break; // bail to prevent buffer overflow
            }

            QByteArray encodedID = dataByteArray.mid((int)processedBytes, NUM_BYTES_RFC4122_UUID);
            QUuid entityID = QUuid::fromRfc4122(encodedID);
            dataAt += encodedID.size();
            processedBytes += encodedID.size();

            #ifdef EXTRA_ERASE_DEBUGGING
                qCDebug(entities) << "    ---- EntityTree::processEraseMessageDetails() contains id:" << entityID;
            #endif

            EntityItemID entityItemID(entityID);

            if (shouldEraseEntity(entityID, sourceNode)) {
                entityItemIDsToDelete << entityItemID;
                cleanupCloneIDs(entityItemID);
            }

        }
        deleteEntities(entityItemIDsToDelete, true, true);
    }
    return (int)processedBytes;
}

EntityTreeElementPointer EntityTree::getContainingElement(const EntityItemID& entityItemID)  /*const*/ {
    EntityItemPointer entity;
    {
        QReadLocker locker(&_entityMapLock);
        entity = _entityMap.value(entityItemID);
    }
    if (entity) {
        return entity->getElement();
    }
    return EntityTreeElementPointer(nullptr);
}

void EntityTree::addEntityMapEntry(EntityItemPointer entity) {
    EntityItemID id = entity->getEntityItemID();
    QWriteLocker locker(&_entityMapLock);
    EntityItemPointer otherEntity = _entityMap.value(id);
    if (otherEntity) {
        qCWarning(entities) << "EntityTree::addEntityMapEntry() found pre-existing id " << id;
        assert(false);
        return;
    }
    _entityMap.insert(id, entity);
}

void EntityTree::clearEntityMapEntry(const EntityItemID& id) {
    QWriteLocker locker(&_entityMapLock);
    _entityMap.remove(id);
}

void EntityTree::debugDumpMap() {
    // QHash's are implicitly shared, so we make a shared copy and use that instead.
    // This way we might be able to avoid both a lock and a true copy.
    QHash<EntityItemID, EntityItemPointer> localMap(_entityMap);
    qCDebug(entities) << "EntityTree::debugDumpMap() --------------------------";
    QHashIterator<EntityItemID, EntityItemPointer> i(localMap);
    while (i.hasNext()) {
        i.next();
        qCDebug(entities) << i.key() << ": " << i.value()->getElement().get();
    }
    qCDebug(entities) << "-----------------------------------------------------";
}

class ContentsDimensionOperator : public RecurseOctreeOperator {
public:
    virtual bool preRecursion(const OctreeElementPointer& element) override;
    virtual bool postRecursion(const OctreeElementPointer& element) override { return true; }
    glm::vec3 getDimensions() const { return _contentExtents.size(); }
    float getLargestDimension() const { return _contentExtents.largestDimension(); }
private:
    Extents _contentExtents;
};

bool ContentsDimensionOperator::preRecursion(const OctreeElementPointer& element) {
    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
    entityTreeElement->expandExtentsToContents(_contentExtents);
    return true;
}

glm::vec3 EntityTree::getContentsDimensions() {
    ContentsDimensionOperator theOperator;
    recurseTreeWithOperator(&theOperator);
    return theOperator.getDimensions();
}

float EntityTree::getContentsLargestDimension() {
    ContentsDimensionOperator theOperator;
    recurseTreeWithOperator(&theOperator);
    return theOperator.getLargestDimension();
}

class DebugOperator : public RecurseOctreeOperator {
public:
    virtual bool preRecursion(const OctreeElementPointer& element) override;
    virtual bool postRecursion(const OctreeElementPointer& element) override { return true; }
};

bool DebugOperator::preRecursion(const OctreeElementPointer& element) {
    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
    qCDebug(entities) << "EntityTreeElement [" << entityTreeElement.get() << "]";
    entityTreeElement->debugDump();
    return true;
}

void EntityTree::dumpTree() {
    DebugOperator theOperator;
    recurseTreeWithOperator(&theOperator);
}

class PruneOperator : public RecurseOctreeOperator {
public:
    virtual bool preRecursion(const OctreeElementPointer& element) override { return true; }
    virtual bool postRecursion(const OctreeElementPointer& element) override;
};

bool PruneOperator::postRecursion(const OctreeElementPointer& element) {
    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
    entityTreeElement->pruneChildren();
    return true;
}

void EntityTree::pruneTree() {
    PruneOperator theOperator;
    recurseTreeWithOperator(&theOperator);
}


QByteArray EntityTree::remapActionDataIDs(QByteArray actionData, QHash<EntityItemID, EntityItemID>& map) {
    if (actionData.isEmpty()) {
        return actionData;
    }

    QDataStream serializedActionsStream(actionData);
    QVector<QByteArray> serializedActions;
    serializedActionsStream >> serializedActions;

    auto actionFactory = DependencyManager::get<EntityDynamicFactoryInterface>();

    QHash<QUuid, EntityDynamicPointer> remappedActions;
    foreach(QByteArray serializedAction, serializedActions) {
        QDataStream serializedActionStream(serializedAction);
        EntityDynamicType actionType;
        QUuid oldActionID;
        serializedActionStream >> actionType;
        serializedActionStream >> oldActionID;
        EntityDynamicPointer action = actionFactory->factoryBA(nullptr, serializedAction);
        if (action) {
            action->remapIDs(map);
            remappedActions[action->getID()] = action;
        }
    }

    QVector<QByteArray> remappedSerializedActions;

    QHash<QUuid, EntityDynamicPointer>::const_iterator i = remappedActions.begin();
    while (i != remappedActions.end()) {
        EntityDynamicPointer action = i.value();
        QByteArray bytesForAction = action->serialize();
        remappedSerializedActions << bytesForAction;
        i++;
    }

    QByteArray result;
    QDataStream remappedSerializedActionsStream(&result, QIODevice::WriteOnly);
    remappedSerializedActionsStream << remappedSerializedActions;
    return result;
}

QVector<EntityItemID> EntityTree::sendEntities(EntityEditPacketSender* packetSender, EntityTreePointer localTree,
                                               float x, float y, float z) {
    SendEntitiesOperationArgs args;
    args.ourTree = this;
    args.otherTree = localTree;
    args.root = glm::vec3(x, y, z);
    // If this is called repeatedly (e.g., multiple pastes with the same data), the new elements will clash unless we
    // use new identifiers.  We need to keep a map so that we can map parent identifiers correctly.
    QHash<EntityItemID, EntityItemID> map;

    args.map = &map;
    withReadLock([&] {
        recurseTreeWithOperation(sendEntitiesOperation, &args);
    });

    // The values from map are used as the list of successfully "sent" entities.  If some didn't actually make it,
    // pull them out.  Bogus entries could happen if part of the imported data makes some reference to an entity
    // that isn't in the data being imported.  For those that made it, fix up their queryAACubes and send an
    // add-entity packet to the server.

    // fix the queryAACubes of any children that were read in before their parents, get them into the correct element
    MovingEntitiesOperator moveOperator;
    QHash<EntityItemID, EntityItemID>::iterator i = map.begin();
    while (i != map.end()) {
        EntityItemID newID = i.value();
        EntityItemPointer entity = localTree->findEntityByEntityItemID(newID);
        if (entity) {
            if (!entity->getParentID().isNull()) {
                addToNeedsParentFixupList(entity);
            }
            entity->forceQueryAACubeUpdate();
            entity->updateQueryAACube();
            moveOperator.addEntityToMoveList(entity, entity->getQueryAACube());
            i++;
        } else {
            i = map.erase(i);
        }
    }
    if (moveOperator.hasMovingEntities()) {
        PerformanceTimer perfTimer("recurseTreeWithOperator");
        localTree->recurseTreeWithOperator(&moveOperator);
    }

    if (!_serverlessDomain) {
        // send add-entity packets to the server
        i = map.begin();
        while (i != map.end()) {
            EntityItemID newID = i.value();
            EntityItemPointer entity = localTree->findEntityByEntityItemID(newID);
            if (entity) {
                // queue the packet to send to the server
                entity->updateQueryAACube();
                EntityItemProperties properties = entity->getProperties();
                properties.markAllChanged(); // so the entire property set is considered new, since we're making a new entity
                packetSender->queueEditEntityMessage(PacketType::EntityAdd, localTree, newID, properties);
                i++;
            } else {
                i = map.erase(i);
            }
        }
        packetSender->releaseQueuedMessages();
    }

    return map.values().toVector();
}

bool EntityTree::sendEntitiesOperation(const OctreeElementPointer& element, void* extraData) {
    SendEntitiesOperationArgs* args = static_cast<SendEntitiesOperationArgs*>(extraData);
    EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);

    auto getMapped = [&args](EntityItemID oldID) {
        if (oldID.isNull()) {
            return EntityItemID();
        }

        QHash<EntityItemID, EntityItemID>::iterator iter = args->map->find(oldID);
        if (iter == args->map->end()) {
            EntityItemID newID;
            if (oldID == AVATAR_SELF_ID) {
                auto nodeList = DependencyManager::get<NodeList>();
                newID = EntityItemID(nodeList->getSessionUUID());
            } else {
                newID = QUuid::createUuid();
            }
            args->map->insert(oldID, newID);
            return newID;
        }
        return iter.value();
    };

    entityTreeElement->forEachEntity([&args, &getMapped, &element](EntityItemPointer item) {
        EntityItemID oldID = item->getEntityItemID();
        EntityItemID newID = getMapped(oldID);
        EntityItemProperties properties = item->getProperties();

        EntityItemID oldParentID = properties.getParentID();
        if (oldParentID.isInvalidID()) {  // no parent
            properties.setPosition(properties.getPosition() + args->root);
        } else {
            EntityItemPointer parentEntity = args->ourTree->findEntityByEntityItemID(oldParentID);
            if (parentEntity || oldParentID == AVATAR_SELF_ID) { // map the parent
                properties.setParentID(getMapped(oldParentID));
                // But do not add root offset in this case.
            } else { // Should not happen, but let's try to be helpful...
                item->globalizeProperties(properties, "Cannot find %3 parent of %2 %1", args->root);
            }
        }

        properties.setXNNeighborID(getMapped(properties.getXNNeighborID()));
        properties.setXPNeighborID(getMapped(properties.getXPNeighborID()));
        properties.setYNNeighborID(getMapped(properties.getYNNeighborID()));
        properties.setYPNeighborID(getMapped(properties.getYPNeighborID()));
        properties.setZNNeighborID(getMapped(properties.getZNNeighborID()));
        properties.setZPNeighborID(getMapped(properties.getZPNeighborID()));

        QByteArray actionData = properties.getActionData();
        properties.setActionData(remapActionDataIDs(actionData, *args->map));

        // set creation time to "now" for imported entities
        properties.setCreated(usecTimestampNow());

        EntityTreeElementPointer entityTreeElement = std::static_pointer_cast<EntityTreeElement>(element);
        EntityTreePointer tree = entityTreeElement->getTree();

        // also update the local tree instantly (note: this is not our tree, but an alternate tree)
        if (args->otherTree) {
            args->otherTree->withWriteLock([&] {
                EntityItemPointer entity = args->otherTree->addEntity(newID, properties);
                if (entity) {
                    entity->deserializeActions();
                }
                // else: there was an error adding this entity
            });
        }
        return newID;
    });

    return true;
}

bool EntityTree::writeToMap(QVariantMap& entityDescription, OctreeElementPointer element, bool skipDefaultValues,
                            bool skipThoseWithBadParents) {
    if (! entityDescription.contains("Entities")) {
        entityDescription["Entities"] = QVariantList();
    }
    entityDescription["DataVersion"] = _persistDataVersion;
    entityDescription["Id"] = _persistID;
    QScriptEngine scriptEngine;
    RecurseOctreeToMapOperator theOperator(entityDescription, element, &scriptEngine, skipDefaultValues,
                                            skipThoseWithBadParents, _myAvatar);
    withReadLock([&] {
        recurseTreeWithOperator(&theOperator);
    });
    return true;
}

void convertGrabUserDataToProperties(EntityItemProperties& properties) {
    GrabPropertyGroup& grabProperties = properties.getGrab();
    QJsonObject userData = QJsonDocument::fromJson(properties.getUserData().toUtf8()).object();

    QJsonValue grabbableKeyValue = userData["grabbableKey"];
    if (grabbableKeyValue.isObject()) {
        QJsonObject grabbableKey = grabbableKeyValue.toObject();

        QJsonValue wantsTrigger = grabbableKey["wantsTrigger"];
        if (wantsTrigger.isBool()) {
            grabProperties.setTriggerable(wantsTrigger.toBool());
        }
        QJsonValue triggerable = grabbableKey["triggerable"];
        if (triggerable.isBool()) {
            grabProperties.setTriggerable(triggerable.toBool());
        }
        QJsonValue grabbable = grabbableKey["grabbable"];
        if (grabbable.isBool()) {
            grabProperties.setGrabbable(grabbable.toBool());
        }
        QJsonValue ignoreIK = grabbableKey["ignoreIK"];
        if (ignoreIK.isBool()) {
            grabProperties.setGrabFollowsController(ignoreIK.toBool());
        }
        QJsonValue kinematic = grabbableKey["kinematic"];
        if (kinematic.isBool()) {
            grabProperties.setGrabKinematic(kinematic.toBool());
        }
        QJsonValue equippable = grabbableKey["equippable"];
        if (equippable.isBool()) {
            grabProperties.setEquippable(equippable.toBool());
        }

        if (grabbableKey["spatialKey"].isObject()) {
            QJsonObject spatialKey = grabbableKey["spatialKey"].toObject();
            grabProperties.setEquippable(true);
            if (spatialKey["leftRelativePosition"].isObject()) {
                grabProperties.setEquippableLeftPosition(qMapToVec3(spatialKey["leftRelativePosition"].toVariant()));
            }
            if (spatialKey["rightRelativePosition"].isObject()) {
                grabProperties.setEquippableRightPosition(qMapToVec3(spatialKey["rightRelativePosition"].toVariant()));
            }
            if (spatialKey["relativeRotation"].isObject()) {
                grabProperties.setEquippableLeftRotation(qMapToQuat(spatialKey["relativeRotation"].toVariant()));
                grabProperties.setEquippableRightRotation(qMapToQuat(spatialKey["relativeRotation"].toVariant()));
            }
        }
    }

    QJsonValue wearableValue = userData["wearable"];
    if (wearableValue.isObject()) {
        QJsonObject wearable = wearableValue.toObject();
        QJsonObject joints = wearable["joints"].toObject();
        if (joints["LeftHand"].isArray()) {
            QJsonArray leftHand = joints["LeftHand"].toArray();
            if (leftHand.size() == 2) {
                grabProperties.setEquippable(true);
                grabProperties.setEquippableLeftPosition(qMapToVec3(leftHand[0].toVariant()));
                grabProperties.setEquippableLeftRotation(qMapToQuat(leftHand[1].toVariant()));
            }
        }
        if (joints["RightHand"].isArray()) {
            QJsonArray rightHand = joints["RightHand"].toArray();
            if (rightHand.size() == 2) {
                grabProperties.setEquippable(true);
                grabProperties.setEquippableRightPosition(qMapToVec3(rightHand[0].toVariant()));
                grabProperties.setEquippableRightRotation(qMapToQuat(rightHand[1].toVariant()));
            }
        }
    }

    QJsonValue equipHotspotsValue = userData["equipHotspots"];
    if (equipHotspotsValue.isArray()) {
        QJsonArray equipHotspots = equipHotspotsValue.toArray();
        if (equipHotspots.size() > 0) {
            // just take the first one
            QJsonObject firstHotSpot = equipHotspots[0].toObject();
            QJsonObject joints = firstHotSpot["joints"].toObject();
            if (joints["LeftHand"].isArray()) {
                QJsonArray leftHand = joints["LeftHand"].toArray();
                if (leftHand.size() == 2) {
                    grabProperties.setEquippableLeftPosition(qMapToVec3(leftHand[0].toVariant()));
                    grabProperties.setEquippableLeftRotation(qMapToQuat(leftHand[1].toVariant()));
                }
            }
            if (joints["RightHand"].isArray()) {
                QJsonArray rightHand = joints["RightHand"].toArray();
                if (rightHand.size() == 2) {
                    grabProperties.setEquippable(true);
                    grabProperties.setEquippableRightPosition(qMapToVec3(rightHand[0].toVariant()));
                    grabProperties.setEquippableRightRotation(qMapToQuat(rightHand[1].toVariant()));
                }
            }
            QJsonValue indicatorURL = firstHotSpot["modelURL"];
            if (indicatorURL.isString()) {
                grabProperties.setEquippableIndicatorURL(indicatorURL.toString());
            }
            QJsonValue indicatorScale = firstHotSpot["modelScale"];
            if (indicatorScale.isDouble()) {
                grabProperties.setEquippableIndicatorScale(glm::vec3((float)indicatorScale.toDouble()));
            } else if (indicatorScale.isObject()) {
                grabProperties.setEquippableIndicatorScale(qMapToVec3(indicatorScale.toVariant()));
            }
            QJsonValue indicatorOffset = firstHotSpot["position"];
            if (indicatorOffset.isObject()) {
                grabProperties.setEquippableIndicatorOffset(qMapToVec3(indicatorOffset.toVariant()));
            }
        }
    }
}


bool EntityTree::readFromMap(QVariantMap& map) {
    // These are needed to deal with older content (before adding inheritance modes)
    int contentVersion = map["Version"].toInt();
    bool needsConversion = (contentVersion < (int)EntityVersion::ZoneLightInheritModes);

    if (map.contains("Id")) {
        _persistID = map["Id"].toUuid();
    }

    if (map.contains("DataVersion")) {
        _persistDataVersion = map["DataVersion"].toInt();
    }

    _namedPaths.clear();
    if (map.contains("Paths")) {
        QVariantMap namedPathsMap = map["Paths"].toMap();
        for(QVariantMap::const_iterator iter = namedPathsMap.begin(); iter != namedPathsMap.end(); ++iter) {
            QString namedPathName = iter.key();
            QString namedPathViewPoint = iter.value().toString();
            _namedPaths[namedPathName] = namedPathViewPoint;
        }
    }

    // map will have a top-level list keyed as "Entities".  This will be extracted
    // and iterated over.  Each member of this list is converted to a QVariantMap, then
    // to a QScriptValue, and then to EntityItemProperties.  These properties are used
    // to add the new entity to the EntityTree.
    QVariantList entitiesQList = map["Entities"].toList();
    QScriptEngine scriptEngine;

    if (entitiesQList.length() == 0) {
        // Empty map or invalidly formed file.
        return false;
    }

    QMap<QUuid, QVector<QUuid>> cloneIDs;

    bool success = true;
    foreach (QVariant entityVariant, entitiesQList) {
        // QVariantMap --> QScriptValue --> EntityItemProperties --> Entity
        QVariantMap entityMap = entityVariant.toMap();

        // handle parentJointName for wearables
        if (_myAvatar && entityMap.contains("parentJointName") && entityMap.contains("parentID") &&
            QUuid(entityMap["parentID"].toString()) == AVATAR_SELF_ID) {

            entityMap["parentJointIndex"] = _myAvatar->getJointIndex(entityMap["parentJointName"].toString());

            qCDebug(entities) << "Found parentJointName " << entityMap["parentJointName"].toString() <<
                " mapped it to parentJointIndex " << entityMap["parentJointIndex"].toInt();
        }

        QScriptValue entityScriptValue = variantMapToScriptValue(entityMap, scriptEngine);
        EntityItemProperties properties;
        EntityItemPropertiesFromScriptValueIgnoreReadOnly(entityScriptValue, properties);

        EntityItemID entityItemID;
        if (entityMap.contains("id")) {
            entityItemID = EntityItemID(QUuid(entityMap["id"].toString()));
        } else {
            entityItemID = EntityItemID(QUuid::createUuid());
        }

        // Convert old clientOnly bool to new entityHostType enum
        // (must happen before setOwningAvatarID below)
        if (contentVersion < (int)EntityVersion::EntityHostTypes) {
            if (entityMap.contains("clientOnly")) {
                properties.setEntityHostType(entityMap["clientOnly"].toBool() ? entity::HostType::AVATAR : entity::HostType::DOMAIN);
            }
        }

        if (properties.getEntityHostType() == entity::HostType::AVATAR) {
            auto nodeList = DependencyManager::get<NodeList>();
            const QUuid myNodeID = nodeList->getSessionUUID();
            properties.setOwningAvatarID(myNodeID);
        }

        // Fix for older content not containing mode fields in the zones
        if (needsConversion && (properties.getType() == EntityTypes::EntityType::Zone)) {
            // The legacy version had no keylight mode - this is set to on
            properties.setKeyLightMode(COMPONENT_MODE_ENABLED);

            // The ambient URL has been moved from "keyLight" to "ambientLight"
            if (entityMap.contains("keyLight")) {
                QVariantMap keyLightObject = entityMap["keyLight"].toMap();
                properties.getAmbientLight().setAmbientURL(keyLightObject["ambientURL"].toString());
            }

            // Copy the skybox URL if the ambient URL is empty, as this is the legacy behaviour
            // Use skybox value only if it is not empty, else set ambientMode to inherit (to use default URL)
            properties.setAmbientLightMode(COMPONENT_MODE_ENABLED);
            if (properties.getAmbientLight().getAmbientURL() == "") {
                if (properties.getSkybox().getURL() != "") {
                    properties.getAmbientLight().setAmbientURL(properties.getSkybox().getURL());
                } else {
                    properties.setAmbientLightMode(COMPONENT_MODE_INHERIT);
                }
            }

            // The background should be enabled if the mode is skybox
            // Note that if the values are default then they are not stored in the JSON file
            if (entityMap.contains("backgroundMode") && (entityMap["backgroundMode"].toString() == "skybox")) {
                properties.setSkyboxMode(COMPONENT_MODE_ENABLED);
            } else {
                properties.setSkyboxMode(COMPONENT_MODE_INHERIT);
            }
        }

        // Convert old materials so that they use materialData instead of userData
        if (contentVersion < (int)EntityVersion::MaterialData && properties.getType() == EntityTypes::EntityType::Material) {
            if (properties.getMaterialURL().startsWith("userData")) {
                QString materialURL = properties.getMaterialURL();
                properties.setMaterialURL(materialURL.replace("userData", "materialData"));

                QJsonObject userData = QJsonDocument::fromJson(properties.getUserData().toUtf8()).object();
                QJsonObject materialData;
                QJsonValue materialVersion = userData["materialVersion"];
                if (!materialVersion.isNull()) {
                    materialData.insert("materialVersion", materialVersion);
                    userData.remove("materialVersion");
                }
                QJsonValue materials = userData["materials"];
                if (!materials.isNull()) {
                    materialData.insert("materials", materials);
                    userData.remove("materials");
                }

                properties.setMaterialData(QJsonDocument(materialData).toJson());
                properties.setUserData(QJsonDocument(userData).toJson());
            }
        }

        // Convert old cloneable entities so they use cloneableData instead of userData
        if (contentVersion < (int)EntityVersion::CloneableData) {
            QJsonObject userData = QJsonDocument::fromJson(properties.getUserData().toUtf8()).object();
            QJsonObject grabbableKey = userData["grabbableKey"].toObject();
            QJsonValue cloneable = grabbableKey["cloneable"];
            if (cloneable.isBool() && cloneable.toBool()) {
                QJsonValue cloneLifetime = grabbableKey["cloneLifetime"];
                QJsonValue cloneLimit = grabbableKey["cloneLimit"];
                QJsonValue cloneDynamic = grabbableKey["cloneDynamic"];
                QJsonValue cloneAvatarEntity = grabbableKey["cloneAvatarEntity"];

                // This is cloneable, we need to convert the properties
                properties.setCloneable(true);
                properties.setCloneLifetime(cloneLifetime.toInt());
                properties.setCloneLimit(cloneLimit.toInt());
                properties.setCloneDynamic(cloneDynamic.toBool());
                properties.setCloneAvatarEntity(cloneAvatarEntity.toBool());
            }
        }

        // convert old grab-related userData to new grab properties
        if (contentVersion < (int)EntityVersion::GrabProperties) {
            convertGrabUserDataToProperties(properties);
        }

        // Zero out the spread values that were fixed in version ParticleEntityFix so they behave the same as before
        if (contentVersion < (int)EntityVersion::ParticleEntityFix) {
            properties.setRadiusSpread(0.0f);
            properties.setAlphaSpread(0.0f);
            properties.setColorSpread({0, 0, 0});
        }

        EntityItemPointer entity = addEntity(entityItemID, properties);
        if (!entity) {
            qCDebug(entities) << "adding Entity failed:" << entityItemID << properties.getType();
            success = false;
        }

        if (entity) {
            const QUuid& cloneOriginID = entity->getCloneOriginID();
            if (!cloneOriginID.isNull()) {
                cloneIDs[cloneOriginID].push_back(entity->getEntityItemID());
            }
        }
    }

    for (const auto& entityID : cloneIDs.keys()) {
        auto entity = findEntityByID(entityID);
        if (entity) {
            entity->setCloneIDs(cloneIDs.value(entityID));
        }
    }

    return success;
}

bool EntityTree::writeToJSON(QString& jsonString, const OctreeElementPointer& element) {
    QScriptEngine scriptEngine;
    RecurseOctreeToJSONOperator theOperator(element, &scriptEngine, jsonString);
    withReadLock([&] {
        recurseTreeWithOperator(&theOperator);
    });

    jsonString = theOperator.getJson();
    return true;
}

void EntityTree::resetClientEditStats() {
    _treeResetTime = usecTimestampNow();
    _maxEditDelta = 0;
    _totalEditDeltas = 0;
    _totalTrackedEdits = 0;
}



void EntityTree::trackIncomingEntityLastEdited(quint64 lastEditedTime, int bytesRead) {
    // we don't want to track all edit deltas, just those edits that have happend
    // since we connected to this domain. This will filter out all previously created
    // content and only track new edits
    if (lastEditedTime > _treeResetTime) {
        quint64 now = usecTimestampNow();
        quint64 sinceEdit = now - lastEditedTime;

        _totalEditDeltas += sinceEdit;
        _totalEditBytes += bytesRead;
        _totalTrackedEdits++;
        if (sinceEdit > _maxEditDelta) {
            _maxEditDelta = sinceEdit;
        }
    }
}

int EntityTree::getJointIndex(const QUuid& entityID, const QString& name) const {
    EntityTree* nonConstThis = const_cast<EntityTree*>(this);
    EntityItemPointer entity = nonConstThis->findEntityByEntityItemID(entityID);
    if (!entity) {
        return -1;
    }
    return entity->getJointIndex(name);
}

QStringList EntityTree::getJointNames(const QUuid& entityID) const {
    EntityTree* nonConstThis = const_cast<EntityTree*>(this);
    EntityItemPointer entity = nonConstThis->findEntityByEntityItemID(entityID);
    if (!entity) {
        return QStringList();
    }
    return entity->getJointNames();
}

std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> EntityTree::_addMaterialToEntityOperator = nullptr;
std::function<bool(const QUuid&, graphics::MaterialPointer, const std::string&)> EntityTree::_removeMaterialFromEntityOperator = nullptr;
std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> EntityTree::_addMaterialToAvatarOperator = nullptr;
std::function<bool(const QUuid&, graphics::MaterialPointer, const std::string&)> EntityTree::_removeMaterialFromAvatarOperator = nullptr;
std::function<bool(const QUuid&, graphics::MaterialLayer, const std::string&)> EntityTree::_addMaterialToOverlayOperator = nullptr;
std::function<bool(const QUuid&, graphics::MaterialPointer, const std::string&)> EntityTree::_removeMaterialFromOverlayOperator = nullptr;

bool EntityTree::addMaterialToEntity(const QUuid& entityID, graphics::MaterialLayer material, const std::string& parentMaterialName) {
    if (_addMaterialToEntityOperator) {
        return _addMaterialToEntityOperator(entityID, material, parentMaterialName);
    }
    return false;
}

bool EntityTree::removeMaterialFromEntity(const QUuid& entityID, graphics::MaterialPointer material, const std::string& parentMaterialName) {
    if (_removeMaterialFromEntityOperator) {
        return _removeMaterialFromEntityOperator(entityID, material, parentMaterialName);
    }
    return false;
}

bool EntityTree::addMaterialToAvatar(const QUuid& avatarID, graphics::MaterialLayer material, const std::string& parentMaterialName) {
    if (_addMaterialToAvatarOperator) {
        return _addMaterialToAvatarOperator(avatarID, material, parentMaterialName);
    }
    return false;
}

bool EntityTree::removeMaterialFromAvatar(const QUuid& avatarID, graphics::MaterialPointer material, const std::string& parentMaterialName) {
    if (_removeMaterialFromAvatarOperator) {
        return _removeMaterialFromAvatarOperator(avatarID, material, parentMaterialName);
    }
    return false;
}

bool EntityTree::addMaterialToOverlay(const QUuid& overlayID, graphics::MaterialLayer material, const std::string& parentMaterialName) {
    if (_addMaterialToOverlayOperator) {
        return _addMaterialToOverlayOperator(overlayID, material, parentMaterialName);
    }
    return false;
}

bool EntityTree::removeMaterialFromOverlay(const QUuid& overlayID, graphics::MaterialPointer material, const std::string& parentMaterialName) {
    if (_removeMaterialFromOverlayOperator) {
        return _removeMaterialFromOverlayOperator(overlayID, material, parentMaterialName);
    }
    return false;
}
