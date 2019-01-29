//
//  AvatarMotionState.h
//  interface/src/avatar/
//
//  Created by Andrew Meadows 2015.05.14
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AvatarMotionState_h
#define hifi_AvatarMotionState_h

#include <QSet>

#include <ObjectMotionState.h>
#include <BulletUtil.h>

#include "OtherAvatar.h"

class AvatarMotionState : public ObjectMotionState {
public:
    AvatarMotionState(OtherAvatarPointer avatar, const btCollisionShape* shape);

    virtual void handleEasyChanges(uint32_t& flags) override;
    virtual bool handleHardAndEasyChanges(uint32_t& flags, PhysicsEngine* engine) override;

    virtual PhysicsMotionType getMotionType() const override { return _motionType; }

    virtual uint32_t getIncomingDirtyFlags() override;
    virtual void clearIncomingDirtyFlags() override;

    virtual PhysicsMotionType computePhysicsMotionType() const override;

    virtual bool isMoving() const override;

    // this relays incoming position/rotation to the RigidBody
    virtual void getWorldTransform(btTransform& worldTrans) const override;

    // this relays outgoing position/rotation to the EntityItem
    virtual void setWorldTransform(const btTransform& worldTrans) override;


    // These pure virtual methods must be implemented for each MotionState type
    // and make it possible to implement more complicated methods in this base class.

    // pure virtual overrides from ObjectMotionState
    virtual float getObjectRestitution() const override;
    virtual float getObjectFriction() const override;
    virtual float getObjectLinearDamping() const override;
    virtual float getObjectAngularDamping() const override;

    virtual glm::vec3 getObjectPosition() const override;
    virtual glm::quat getObjectRotation() const override;
    virtual glm::vec3 getObjectLinearVelocity() const override;
    virtual glm::vec3 getObjectAngularVelocity() const override;
    virtual glm::vec3 getObjectGravity() const override;

    virtual const QUuid getObjectID() const override;

    virtual QString getName() const override;
    virtual QUuid getSimulatorID() const override;

    void setBoundingBox(const glm::vec3& corner, const glm::vec3& diagonal);

    void addDirtyFlags(uint32_t flags) { _dirtyFlags |= flags; }

    void setCollisionGroup(int32_t group) { _collisionGroup = group; }
    int32_t getCollisionGroup() { return _collisionGroup; }

    virtual void computeCollisionGroupAndMask(int32_t& group, int32_t& mask) const override;

    virtual float getMass() const override;

    friend class AvatarManager;
    friend class Avatar;

protected:
    void setRigidBody(btRigidBody* body) override;
    void setShape(const btCollisionShape* shape) override;
    void cacheShapeDiameter();

    // the dtor had been made protected to force the compiler to verify that it is only
    // ever called by the Avatar class dtor.
    ~AvatarMotionState();

    virtual bool isReadyToComputeShape() const override { return true; }
    virtual const btCollisionShape* computeNewShape() override;

    OtherAvatarPointer _avatar;
    float _diameter { 0.0f };
    int32_t _collisionGroup;
    uint32_t _dirtyFlags;
};

#endif // hifi_AvatarMotionState_h
