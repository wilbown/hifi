//
//  Created by Bradley Austin Davis on 2015/05/12
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "WebEntityItem.h"

#include <glm/gtx/transform.hpp>

#include <QDebug>
#include <QJsonDocument>

#include <ByteCountCoding.h>
#include <GeometryUtil.h>

#include "EntitiesLogging.h"
#include "EntityItemProperties.h"
#include "EntityTree.h"
#include "EntityTreeElement.h"

const QString WebEntityItem::DEFAULT_SOURCE_URL = "http://www.google.com";
const uint8_t WebEntityItem::DEFAULT_MAX_FPS = 10;

EntityItemPointer WebEntityItem::factory(const EntityItemID& entityID, const EntityItemProperties& properties) {
    EntityItemPointer entity(new WebEntityItem(entityID), [](EntityItem* ptr) { ptr->deleteLater(); });
    entity->setProperties(properties);
    return entity;
}

WebEntityItem::WebEntityItem(const EntityItemID& entityItemID) : EntityItem(entityItemID) {
    _type = EntityTypes::Web;
}

void WebEntityItem::setUnscaledDimensions(const glm::vec3& value) {
    // NOTE: Web Entities always have a "depth" of 1cm.
    const float WEB_ENTITY_ITEM_FIXED_DEPTH = 0.01f;
    EntityItem::setUnscaledDimensions(glm::vec3(value.x, value.y, WEB_ENTITY_ITEM_FIXED_DEPTH));
}

EntityItemProperties WebEntityItem::getProperties(const EntityPropertyFlags& desiredProperties, bool allowEmptyDesiredProperties) const {
    EntityItemProperties properties = EntityItem::getProperties(desiredProperties, allowEmptyDesiredProperties); // get the properties from our base class

    COPY_ENTITY_PROPERTY_TO_PROPERTIES(color, getColor);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(alpha, getAlpha);
    withReadLock([&] {
        _pulseProperties.getProperties(properties);
    });

    COPY_ENTITY_PROPERTY_TO_PROPERTIES(sourceUrl, getSourceUrl);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(dpi, getDPI);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(scriptURL, getScriptURL);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(maxFPS, getMaxFPS);
    COPY_ENTITY_PROPERTY_TO_PROPERTIES(inputMode, getInputMode);
    return properties;
}

bool WebEntityItem::setProperties(const EntityItemProperties& properties) {
    bool somethingChanged = false;
    somethingChanged = EntityItem::setProperties(properties); // set the properties in our base class

    SET_ENTITY_PROPERTY_FROM_PROPERTIES(color, setColor);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(alpha, setAlpha);
    withWriteLock([&] {
        bool pulsePropertiesChanged = _pulseProperties.setProperties(properties);
        somethingChanged |= pulsePropertiesChanged;
    });

    SET_ENTITY_PROPERTY_FROM_PROPERTIES(sourceUrl, setSourceUrl);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(dpi, setDPI);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(scriptURL, setScriptURL);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(maxFPS, setMaxFPS);
    SET_ENTITY_PROPERTY_FROM_PROPERTIES(inputMode, setInputMode);

    if (somethingChanged) {
        bool wantDebug = false;
        if (wantDebug) {
            uint64_t now = usecTimestampNow();
            int elapsed = now - getLastEdited();
            qCDebug(entities) << "WebEntityItem::setProperties() AFTER update... edited AGO=" << elapsed <<
                    "now=" << now << " getLastEdited()=" << getLastEdited();
        }
        setLastEdited(properties._lastEdited);
    }

    return somethingChanged;
}

int WebEntityItem::readEntitySubclassDataFromBuffer(const unsigned char* data, int bytesLeftToRead,
                                                ReadBitstreamToTreeParams& args,
                                                EntityPropertyFlags& propertyFlags, bool overwriteLocalData,
                                                bool& somethingChanged) {

    int bytesRead = 0;
    const unsigned char* dataAt = data;

    READ_ENTITY_PROPERTY(PROP_COLOR, glm::u8vec3, setColor);
    READ_ENTITY_PROPERTY(PROP_ALPHA, float, setAlpha);
    withWriteLock([&] {
        int bytesFromPulse = _pulseProperties.readEntitySubclassDataFromBuffer(dataAt, (bytesLeftToRead - bytesRead), args,
            propertyFlags, overwriteLocalData,
            somethingChanged);
        bytesRead += bytesFromPulse;
        dataAt += bytesFromPulse;
    });

    READ_ENTITY_PROPERTY(PROP_SOURCE_URL, QString, setSourceUrl);
    READ_ENTITY_PROPERTY(PROP_DPI, uint16_t, setDPI);
    READ_ENTITY_PROPERTY(PROP_SCRIPT_URL, QString, setScriptURL);
    READ_ENTITY_PROPERTY(PROP_MAX_FPS, uint8_t, setMaxFPS);
    READ_ENTITY_PROPERTY(PROP_INPUT_MODE, WebInputMode, setInputMode);

    return bytesRead;
}

EntityPropertyFlags WebEntityItem::getEntityProperties(EncodeBitstreamParams& params) const {
    EntityPropertyFlags requestedProperties = EntityItem::getEntityProperties(params);
    requestedProperties += PROP_COLOR;
    requestedProperties += PROP_ALPHA;
    requestedProperties += _pulseProperties.getEntityProperties(params);

    requestedProperties += PROP_SOURCE_URL;
    requestedProperties += PROP_DPI;
    requestedProperties += PROP_SCRIPT_URL;
    requestedProperties += PROP_MAX_FPS;
    requestedProperties += PROP_INPUT_MODE;
    return requestedProperties;
}

void WebEntityItem::appendSubclassData(OctreePacketData* packetData, EncodeBitstreamParams& params,
                                    EntityTreeElementExtraEncodeDataPointer entityTreeElementExtraEncodeData,
                                    EntityPropertyFlags& requestedProperties,
                                    EntityPropertyFlags& propertyFlags,
                                    EntityPropertyFlags& propertiesDidntFit,
                                    int& propertyCount,
                                    OctreeElement::AppendState& appendState) const {

    bool successPropertyFits = true;
    APPEND_ENTITY_PROPERTY(PROP_COLOR, getColor());
    APPEND_ENTITY_PROPERTY(PROP_ALPHA, getAlpha());
    withReadLock([&] {
        _pulseProperties.appendSubclassData(packetData, params, entityTreeElementExtraEncodeData, requestedProperties,
            propertyFlags, propertiesDidntFit, propertyCount, appendState);
    });

    APPEND_ENTITY_PROPERTY(PROP_SOURCE_URL, getSourceUrl());
    APPEND_ENTITY_PROPERTY(PROP_DPI, getDPI());
    APPEND_ENTITY_PROPERTY(PROP_SCRIPT_URL, getScriptURL());
    APPEND_ENTITY_PROPERTY(PROP_MAX_FPS, getMaxFPS());
    APPEND_ENTITY_PROPERTY(PROP_INPUT_MODE, (uint32_t)getInputMode());
}

bool WebEntityItem::findDetailedRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                                                OctreeElementPointer& element, float& distance,
                                                BoxFace& face, glm::vec3& surfaceNormal,
                                                QVariantMap& extraInfo, bool precisionPicking) const {
    glm::vec3 dimensions = getScaledDimensions();
    glm::vec2 xyDimensions(dimensions.x, dimensions.y);
    glm::quat rotation = getWorldOrientation();
    glm::vec3 position = getWorldPosition() + rotation * (dimensions * (ENTITY_ITEM_DEFAULT_REGISTRATION_POINT - getRegistrationPoint()));

    if (findRayRectangleIntersection(origin, direction, rotation, position, xyDimensions, distance)) {
        glm::vec3 forward = rotation * Vectors::FRONT;
        if (glm::dot(forward, direction) > 0.0f) {
            face = MAX_Z_FACE;
            surfaceNormal = -forward;
        } else {
            face = MIN_Z_FACE;
            surfaceNormal = forward;
        }
        return true;
    } else {
        return false;
    }
}

bool WebEntityItem::findDetailedParabolaIntersection(const glm::vec3& origin, const glm::vec3& velocity, const glm::vec3& acceleration,
                                                     OctreeElementPointer& element, float& parabolicDistance,
                                                     BoxFace& face, glm::vec3& surfaceNormal,
                                                     QVariantMap& extraInfo, bool precisionPicking) const {
    glm::vec3 dimensions = getScaledDimensions();
    glm::vec2 xyDimensions(dimensions.x, dimensions.y);
    glm::quat rotation = getWorldOrientation();
    glm::vec3 position = getWorldPosition() + rotation * (dimensions * (ENTITY_ITEM_DEFAULT_REGISTRATION_POINT - getRegistrationPoint()));

    glm::quat inverseRot = glm::inverse(rotation);
    glm::vec3 localOrigin = inverseRot * (origin - position);
    glm::vec3 localVelocity = inverseRot * velocity;
    glm::vec3 localAcceleration = inverseRot * acceleration;

    if (findParabolaRectangleIntersection(localOrigin, localVelocity, localAcceleration, xyDimensions, parabolicDistance)) {
        float localIntersectionVelocityZ = localVelocity.z + localAcceleration.z * parabolicDistance;
        glm::vec3 forward = rotation * Vectors::FRONT;
        if (localIntersectionVelocityZ > 0.0f) {
            face = MIN_Z_FACE;
            surfaceNormal = forward;
        } else {
            face = MAX_Z_FACE;
            surfaceNormal = -forward;
        }
        return true;
    } else {
        return false;
    }
}

void WebEntityItem::setColor(const glm::u8vec3& value) {
    withWriteLock([&] {
        _color = value;
    });
}

glm::u8vec3 WebEntityItem::getColor() const {
    return resultWithReadLock<glm::u8vec3>([&] {
        return _color;
    });
}

void WebEntityItem::setAlpha(float alpha) {
    withWriteLock([&] {
        _alpha = alpha;
    });
}

float WebEntityItem::getAlpha() const {
    return resultWithReadLock<float>([&] {
        return _alpha;
    });
}

void WebEntityItem::setSourceUrl(const QString& value) {
    withWriteLock([&] {
        if (_sourceUrl != value) {
            auto newURL = QUrl::fromUserInput(value);

            if (newURL.isValid()) {
                _sourceUrl = newURL.toDisplayString();
            } else {
                qCDebug(entities) << "Clearing web entity source URL since" << value << "cannot be parsed to a valid URL.";
            }
        }
    });
}

QString WebEntityItem::getSourceUrl() const { 
    return resultWithReadLock<QString>([&] {
        return _sourceUrl;
    });
}

void WebEntityItem::setDPI(uint16_t value) {
    withWriteLock([&] {
        _dpi = value;
    });
}

uint16_t WebEntityItem::getDPI() const {
    return resultWithReadLock<uint16_t>([&] {
        return _dpi;
    });
}

void WebEntityItem::setScriptURL(const QString& value) {
    withWriteLock([&] {
        if (_scriptURL != value) {
            auto newURL = QUrl::fromUserInput(value);

            if (newURL.isValid()) {
                _scriptURL = newURL.toDisplayString();
            } else {
                qCDebug(entities) << "Clearing web entity source URL since" << value << "cannot be parsed to a valid URL.";
            }
        }
    });
}

QString WebEntityItem::getScriptURL() const {
    return resultWithReadLock<QString>([&] {
        return _scriptURL;
    });
}

void WebEntityItem::setMaxFPS(uint8_t value) {
    withWriteLock([&] {
        _maxFPS = value;
    });
}

uint8_t WebEntityItem::getMaxFPS() const {
    return resultWithReadLock<uint8_t>([&] {
        return _maxFPS;
    });
}

void WebEntityItem::setInputMode(const WebInputMode& value) {
    withWriteLock([&] {
        _inputMode = value;
    });
}

WebInputMode WebEntityItem::getInputMode() const {
    return resultWithReadLock<WebInputMode>([&] {
        return _inputMode;
    });
}

PulsePropertyGroup WebEntityItem::getPulseProperties() const {
    return resultWithReadLock<PulsePropertyGroup>([&] {
        return _pulseProperties;
    });
}