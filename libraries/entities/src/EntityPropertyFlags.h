//
//  EntityPropertyFlags.h
//  libraries/entities/src
//
//  Created by Brad Hefta-Gaub on 12/4/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_EntityPropertyFlags_h
#define hifi_EntityPropertyFlags_h

#include <PropertyFlags.h>

enum EntityPropertyList {
    PROP_PAGED_PROPERTY,
    PROP_CUSTOM_PROPERTIES_INCLUDED,

    // these properties are supported by the EntityItem base class
    PROP_VISIBLE,
    PROP_CAN_CAST_SHADOW,
    PROP_POSITION,
    PROP_DIMENSIONS,
    PROP_ROTATION,
    PROP_DENSITY,
    PROP_VELOCITY,
    PROP_GRAVITY,
    PROP_DAMPING,
    PROP_LIFETIME,
    PROP_SCRIPT,

    // these properties are supported by some derived classes
    PROP_COLOR,

    // these are used by models only
    PROP_MODEL_URL,
    PROP_ANIMATION_URL,
    PROP_ANIMATION_FPS,
    PROP_ANIMATION_FRAME_INDEX,
    PROP_ANIMATION_PLAYING,
    PROP_ANIMATION_ALLOW_TRANSLATION,
    PROP_RELAY_PARENT_JOINTS,

    // these properties are supported by the EntityItem base class
    PROP_REGISTRATION_POINT,
    PROP_ANGULAR_VELOCITY,
    PROP_ANGULAR_DAMPING,
    PROP_COLLISIONLESS,
    PROP_DYNAMIC, // 24

    // property used by Light entity
    PROP_IS_SPOTLIGHT,
    PROP_DIFFUSE_COLOR,
    PROP_AMBIENT_COLOR_UNUSED, // FIXME - No longer used, can remove and bump protocol
    PROP_SPECULAR_COLOR_UNUSED, // FIXME - No longer used, can remove and bump protocol
    PROP_INTENSITY, // Previously PROP_CONSTANT_ATTENUATION
    PROP_LINEAR_ATTENUATION_UNUSED,
    PROP_QUADRATIC_ATTENUATION_UNUSED,
    PROP_EXPONENT,
    PROP_CUTOFF,

    // available to all entities
    PROP_LOCKED,  // 34

    PROP_TEXTURES,  // used by Model entities
    PROP_ANIMATION_SETTINGS_UNUSED,  // FIXME - No longer used, can remove and bump protocol
    PROP_USER_DATA,  // all entities -- 37
    PROP_SHAPE_TYPE, // used by Model + zones entities

    // used by ParticleEffect entities
    PROP_MAX_PARTICLES, // 39
    PROP_LIFESPAN, // 40 -- used by all entities
    PROP_EMIT_RATE,
    PROP_EMIT_SPEED,
    PROP_EMIT_STRENGTH,
    PROP_EMIT_ACCELERATION, // FIXME - doesn't seem to get set in mark all changed????
    PROP_PARTICLE_RADIUS,  // 45!!

    PROP_COMPOUND_SHAPE_URL, // used by Model + zones entities
    PROP_MARKETPLACE_ID, // all entities
    PROP_ACCELERATION, // all entities
    PROP_SIMULATION_OWNER, // formerly known as PROP_SIMULATOR_ID
    PROP_NAME, // all entities -- 50
    PROP_COLLISION_SOUND_URL,
    PROP_RESTITUTION,
    PROP_FRICTION, // 53

    PROP_VOXEL_VOLUME_SIZE,
    PROP_VOXEL_DATA,
    PROP_VOXEL_SURFACE_STYLE,

    //for lines
    PROP_LINE_WIDTH,
    PROP_LINE_POINTS,

    // used by hyperlinks
    PROP_HREF,
    PROP_DESCRIPTION, // 61

    PROP_FACE_CAMERA,
    PROP_SCRIPT_TIMESTAMP,

    PROP_ACTION_DATA,

    PROP_X_TEXTURE_URL, // used by PolyVox
    PROP_Y_TEXTURE_URL, // used by PolyVox
    PROP_Z_TEXTURE_URL, // used by PolyVox

    // Used by PolyLine entity
    PROP_NORMALS,
    PROP_STROKE_COLORS,
    PROP_STROKE_WIDTHS,
    PROP_IS_UV_MODE_STRETCH,

    // used by particles
    PROP_SPEED_SPREAD,
    PROP_ACCELERATION_SPREAD,

    PROP_X_N_NEIGHBOR_ID, // used by PolyVox
    PROP_Y_N_NEIGHBOR_ID, // used by PolyVox
    PROP_Z_N_NEIGHBOR_ID, // used by PolyVox
    PROP_X_P_NEIGHBOR_ID, // used by PolyVox
    PROP_Y_P_NEIGHBOR_ID, // used by PolyVox
    PROP_Z_P_NEIGHBOR_ID, // used by PolyVox

    // Used by particles
    PROP_RADIUS_SPREAD,
    PROP_RADIUS_START,
    PROP_RADIUS_FINISH,

    PROP_ALPHA,  // Supported by some derived classes

    //Used by particles
    PROP_COLOR_SPREAD,
    PROP_COLOR_START,
    PROP_COLOR_FINISH,
    PROP_ALPHA_SPREAD,
    PROP_ALPHA_START,
    PROP_ALPHA_FINISH,
    PROP_EMIT_ORIENTATION,
    PROP_EMIT_DIMENSIONS,
    PROP_EMIT_RADIUS_START,
    PROP_POLAR_START,
    PROP_POLAR_FINISH,
    PROP_AZIMUTH_START,
    PROP_AZIMUTH_FINISH,

    PROP_ANIMATION_LOOP,
    PROP_ANIMATION_FIRST_FRAME,
    PROP_ANIMATION_LAST_FRAME,
    PROP_ANIMATION_HOLD,
    PROP_ANIMATION_START_AUTOMATICALLY,

    PROP_EMITTER_SHOULD_TRAIL,

    PROP_PARENT_ID,
    PROP_PARENT_JOINT_INDEX,

    PROP_LOCAL_POSITION, // only used to convert values to and from scripts
    PROP_LOCAL_ROTATION, // only used to convert values to and from scripts

    PROP_QUERY_AA_CUBE, // how the EntityTree considers the size and position on an entity

    // ModelEntity joint state
    PROP_JOINT_ROTATIONS_SET,
    PROP_JOINT_ROTATIONS,
    PROP_JOINT_TRANSLATIONS_SET,
    PROP_JOINT_TRANSLATIONS,

    PROP_COLLISION_MASK, // one byte of collision group flags

    PROP_FALLOFF_RADIUS, // for Light entity

    PROP_FLYING_ALLOWED, // can avatars in a zone fly?
    PROP_GHOSTING_ALLOWED, // can avatars in a zone turn off physics?

    PROP_ENTITY_HOST_TYPE, // doesn't go over wire
    PROP_OWNING_AVATAR_ID, // doesn't go over wire

    PROP_SHAPE,
    PROP_DPI,

    PROP_LOCAL_VELOCITY, // only used to convert values to and from scripts
    PROP_LOCAL_ANGULAR_VELOCITY, // only used to convert values to and from scripts

    PROP_LAST_EDITED_BY,

    PROP_SERVER_SCRIPTS,

    PROP_FILTER_URL,

    // Certificable Properties
    PROP_ITEM_NAME,
    PROP_ITEM_DESCRIPTION,
    PROP_ITEM_CATEGORIES,
    PROP_ITEM_ARTIST,
    PROP_ITEM_LICENSE,
    PROP_LIMITED_RUN,
    // PROP_MARKETPLACE_ID is above
    PROP_EDITION_NUMBER,
    PROP_ENTITY_INSTANCE_NUMBER,
    PROP_CERTIFICATE_ID,
    PROP_STATIC_CERTIFICATE_VERSION,

    PROP_CLONEABLE,
    PROP_CLONE_LIFETIME,
    PROP_CLONE_LIMIT,
    PROP_CLONE_DYNAMIC,
    PROP_CLONE_AVATAR_ENTITY,
    PROP_CLONE_ORIGIN_ID,

    PROP_HAZE_MODE,

    PROP_KEYLIGHT_COLOR,
    PROP_KEYLIGHT_INTENSITY,
    PROP_KEYLIGHT_DIRECTION,
    PROP_KEYLIGHT_CAST_SHADOW,

    PROP_HAZE_RANGE,
    PROP_HAZE_COLOR,
    PROP_HAZE_GLARE_COLOR,
    PROP_HAZE_ENABLE_GLARE,
    PROP_HAZE_GLARE_ANGLE,

    PROP_HAZE_ALTITUDE_EFFECT,
    PROP_HAZE_CEILING,
    PROP_HAZE_BASE_REF,

    PROP_HAZE_BACKGROUND_BLEND,

    PROP_HAZE_ATTENUATE_KEYLIGHT,
    PROP_HAZE_KEYLIGHT_RANGE,
    PROP_HAZE_KEYLIGHT_ALTITUDE,

    PROP_KEY_LIGHT_MODE,
    PROP_AMBIENT_LIGHT_MODE,
    PROP_SKYBOX_MODE,

    PROP_LOCAL_DIMENSIONS, // only used to convert values to and from scripts

    PROP_MATERIAL_URL,
    PROP_MATERIAL_MAPPING_MODE,
    PROP_MATERIAL_PRIORITY,
    PROP_PARENT_MATERIAL_NAME,
    PROP_MATERIAL_MAPPING_POS,
    PROP_MATERIAL_MAPPING_SCALE,
    PROP_MATERIAL_MAPPING_ROT,
    PROP_MATERIAL_DATA,

    PROP_VISIBLE_IN_SECONDARY_CAMERA, // not sent over the wire, only used locally

    PROP_PARTICLE_SPIN,
    PROP_SPIN_START,
    PROP_SPIN_FINISH,
    PROP_SPIN_SPREAD,
    PROP_PARTICLE_ROTATE_WITH_ENTITY,

    PROP_BLOOM_MODE,
    PROP_BLOOM_INTENSITY,
    PROP_BLOOM_THRESHOLD,
    PROP_BLOOM_SIZE,

    PROP_GRAB_GRABBABLE,
    PROP_GRAB_KINEMATIC,
    PROP_GRAB_FOLLOWS_CONTROLLER,
    PROP_GRAB_TRIGGERABLE,
    PROP_GRAB_EQUIPPABLE,
    PROP_GRAB_LEFT_EQUIPPABLE_POSITION_OFFSET,
    PROP_GRAB_LEFT_EQUIPPABLE_ROTATION_OFFSET,
    PROP_GRAB_RIGHT_EQUIPPABLE_POSITION_OFFSET,
    PROP_GRAB_RIGHT_EQUIPPABLE_ROTATION_OFFSET,
    PROP_GRAB_EQUIPPABLE_INDICATOR_URL,
    PROP_GRAB_EQUIPPABLE_INDICATOR_SCALE,
    PROP_GRAB_EQUIPPABLE_INDICATOR_OFFSET,

    PROP_MATERIAL_REPEAT,

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // ATTENTION: add new properties to end of list just ABOVE this line
    PROP_AFTER_LAST_ITEM,
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNING! Do not add props here unless you intentionally mean to reuse PROP_ indexes
    //
    // These properties of TextEntity piggy back off of properties of ModelEntities, the type doesn't matter
    // since the derived class knows how to interpret it's own properties and knows the types it expects
    PROP_TEXT_COLOR = PROP_COLOR,
    PROP_TEXT = PROP_MODEL_URL,
    PROP_LINE_HEIGHT = PROP_ANIMATION_URL,
    PROP_BACKGROUND_COLOR = PROP_ANIMATION_FPS,
    PROP_COLLISION_MODEL_URL_OLD_VERSION = PROP_ANIMATION_FPS + 1,

    // Aliases/Piggyback properties for Zones. These properties intentionally reuse the enum values for
    // other properties which will never overlap with each other. We do this so that we don't have to expand
    // the size of the properties bitflags mask
    PROP_SKYBOX_COLOR = PROP_ANIMATION_URL,
    PROP_SKYBOX_URL = PROP_ANIMATION_FPS,

    PROP_AMBIENT_LIGHT_INTENSITY = PROP_CUTOFF,
    PROP_AMBIENT_LIGHT_URL = PROP_ANIMATION_PLAYING,

    // Aliases/Piggyback properties for Web. These properties intentionally reuse the enum values for
    // other properties which will never overlap with each other. 
    PROP_SOURCE_URL = PROP_MODEL_URL,

    // Aliases/Piggyback properties for Particle Emmitter. These properties intentionally reuse the enum values for
    // other properties which will never overlap with each other. 
    PROP_EMITTING_PARTICLES = PROP_ANIMATION_PLAYING,

    // WARNING!!! DO NOT ADD PROPS_xxx here unless you really really meant to.... Add them UP above
};

typedef PropertyFlags<EntityPropertyList> EntityPropertyFlags;

// this is set at the top of EntityItemProperties.cpp to PROP_AFTER_LAST_ITEM - 1.  PROP_AFTER_LAST_ITEM is always
// one greater than the last item property due to the enum's auto-incrementing.
extern EntityPropertyList PROP_LAST_ITEM;

QString EntityPropertyFlagsToString(EntityPropertyFlags propertiesFlags);


QDebug& operator<<(QDebug& dbg, const EntityPropertyFlags& f);


#endif // hifi_EntityPropertyFlags_h
