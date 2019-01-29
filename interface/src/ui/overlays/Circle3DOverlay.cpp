//
//  Circle3DOverlay.cpp
//  interface/src/ui/overlays
//
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Circle3DOverlay.h"

#include <GeometryUtil.h>
#include <GeometryCache.h>
#include <RegisteredMetaTypes.h>

QString const Circle3DOverlay::TYPE = "circle3d";

Circle3DOverlay::Circle3DOverlay() {}

Circle3DOverlay::Circle3DOverlay(const Circle3DOverlay* circle3DOverlay) :
    Planar3DOverlay(circle3DOverlay),
    _startAt(circle3DOverlay->_startAt),
    _endAt(circle3DOverlay->_endAt),
    _outerRadius(circle3DOverlay->_outerRadius),
    _innerRadius(circle3DOverlay->_innerRadius),
    _innerStartColor(circle3DOverlay->_innerStartColor),
    _innerEndColor(circle3DOverlay->_innerEndColor),
    _outerStartColor(circle3DOverlay->_outerStartColor),
    _outerEndColor(circle3DOverlay->_outerEndColor),
    _innerStartAlpha(circle3DOverlay->_innerStartAlpha),
    _innerEndAlpha(circle3DOverlay->_innerEndAlpha),
    _outerStartAlpha(circle3DOverlay->_outerStartAlpha),
    _outerEndAlpha(circle3DOverlay->_outerEndAlpha),
    _hasTickMarks(circle3DOverlay->_hasTickMarks),
    _majorTickMarksAngle(circle3DOverlay->_majorTickMarksAngle),
    _minorTickMarksAngle(circle3DOverlay->_minorTickMarksAngle),
    _majorTickMarksLength(circle3DOverlay->_majorTickMarksLength),
    _minorTickMarksLength(circle3DOverlay->_minorTickMarksLength),
    _majorTickMarksColor(circle3DOverlay->_majorTickMarksColor),
    _minorTickMarksColor(circle3DOverlay->_minorTickMarksColor)
{
}

Circle3DOverlay::~Circle3DOverlay() {
    auto geometryCache = DependencyManager::get<GeometryCache>();
    if (geometryCache) {
        if (_quadVerticesID) {
            geometryCache->releaseID(_quadVerticesID);
        }
        if (_lineVerticesID) {
            geometryCache->releaseID(_lineVerticesID);
        }
        if (_majorTicksVerticesID) {
            geometryCache->releaseID(_majorTicksVerticesID);
        }
        if (_minorTicksVerticesID) {
            geometryCache->releaseID(_minorTicksVerticesID);
        }
    }
}
void Circle3DOverlay::render(RenderArgs* args) {
    if (!_renderVisible) {
        return; // do nothing if we're not visible
    }

    float alpha = getAlpha();
    if (alpha == 0.0f) {
        return; // do nothing if our alpha is 0, we're not visible
    }

    bool geometryChanged = _dirty;
    _dirty = false;

    const float FULL_CIRCLE = 360.0f;
    const float SLICES = 180.0f;  // The amount of segment to create the circle
    const float SLICE_ANGLE_RADIANS = glm::radians(FULL_CIRCLE / SLICES);

    auto geometryCache = DependencyManager::get<GeometryCache>();

    Q_ASSERT(args->_batch);
    auto& batch = *args->_batch;

    DependencyManager::get<GeometryCache>()->bindSimpleProgram(batch, false, isTransparent(), false, !getIsSolid(), true);

    batch.setModelTransform(getRenderTransform());

    // for our overlay, is solid means we draw a ring between the inner and outer radius of the circle, otherwise
    // we just draw a line...
    if (getIsSolid()) {
        if (!_quadVerticesID) {
            _quadVerticesID = geometryCache->allocateID();
        }

        if (geometryChanged) {
            QVector<glm::vec2> points;
            QVector<glm::vec4> colors;

            float pulseLevel = updatePulse();
            vec4 pulseModifier = vec4(1);
            if (_alphaPulse != 0.0f) {
                pulseModifier.a = (_alphaPulse >= 0.0f) ? pulseLevel : (1.0f - pulseLevel);
            }
            if (_colorPulse != 0.0f) {
                float pulseValue = (_colorPulse >= 0.0f) ? pulseLevel : (1.0f - pulseLevel);
                pulseModifier = vec4(vec3(pulseValue), pulseModifier.a);
            }
            vec4 innerStartColor = vec4(toGlm(_innerStartColor), _innerStartAlpha) * pulseModifier;
            vec4 outerStartColor = vec4(toGlm(_outerStartColor), _outerStartAlpha) * pulseModifier;
            vec4 innerEndColor = vec4(toGlm(_innerEndColor), _innerEndAlpha) * pulseModifier;
            vec4 outerEndColor = vec4(toGlm(_outerEndColor), _outerEndAlpha) * pulseModifier;

            const auto startAtRadians = glm::radians(_startAt);
            const auto endAtRadians = glm::radians(_endAt);

            const auto totalRange = _endAt - _startAt;
            if (_innerRadius <= 0) {
                _solidPrimitive = gpu::TRIANGLE_FAN;
                points << vec2();
                colors << innerStartColor;
                for (float angleRadians = startAtRadians; angleRadians < endAtRadians; angleRadians += SLICE_ANGLE_RADIANS) {
                    float range = (angleRadians - startAtRadians) / totalRange;
                    points << glm::vec2(cosf(angleRadians) * _outerRadius, sinf(angleRadians) * _outerRadius);
                    colors << glm::mix(outerStartColor, outerEndColor, range);
                }
                points << glm::vec2(cosf(endAtRadians) * _outerRadius, sinf(endAtRadians) * _outerRadius);
                colors << outerEndColor;

            } else {
                _solidPrimitive = gpu::TRIANGLE_STRIP;
                for (float angleRadians = startAtRadians; angleRadians < endAtRadians; angleRadians += SLICE_ANGLE_RADIANS) {
                    float range = (angleRadians - startAtRadians) / totalRange;

                    points << glm::vec2(cosf(angleRadians) * _innerRadius, sinf(angleRadians) * _innerRadius);
                    colors << glm::mix(innerStartColor, innerEndColor, range);

                    points << glm::vec2(cosf(angleRadians) * _outerRadius, sinf(angleRadians) * _outerRadius);
                    colors << glm::mix(outerStartColor, outerEndColor, range);
                }
                points << glm::vec2(cosf(endAtRadians) * _innerRadius, sinf(endAtRadians) * _innerRadius);
                colors << innerEndColor;

                points << glm::vec2(cosf(endAtRadians) * _outerRadius, sinf(endAtRadians) * _outerRadius);
                colors << outerEndColor;
            }
            geometryCache->updateVertices(_quadVerticesID, points, colors);
        }
        
        geometryCache->renderVertices(batch, _solidPrimitive, _quadVerticesID);
        
    } else {
        if (!_lineVerticesID) {
            _lineVerticesID = geometryCache->allocateID();
        }
        
        if (geometryChanged) {
            QVector<glm::vec2> points;
            
            const auto startAtRadians = glm::radians(_startAt);
            const auto endAtRadians = glm::radians(_endAt);

            float angleRadians = startAtRadians;
            glm::vec2 firstPoint(cosf(angleRadians) * _outerRadius, sinf(angleRadians) * _outerRadius);
            points << firstPoint;
            
            while (angleRadians < endAtRadians) {
                angleRadians += SLICE_ANGLE_RADIANS;
                glm::vec2 thisPoint(cosf(angleRadians) * _outerRadius, sinf(angleRadians) * _outerRadius);
                points << thisPoint;
                
                if (getIsDashedLine()) {
                    angleRadians += SLICE_ANGLE_RADIANS / 2.0f; // short gap
                    glm::vec2 dashStartPoint(cosf(angleRadians) * _outerRadius, sinf(angleRadians) * _outerRadius);
                    points << dashStartPoint;
                }
            }
            
            // get the last slice portion....
            angleRadians = endAtRadians;
            glm::vec2 lastPoint(cosf(angleRadians) * _outerRadius, sinf(angleRadians) * _outerRadius);
            points << lastPoint;
            geometryCache->updateVertices(_lineVerticesID, points, vec4(toGlm(getColor()), getAlpha()));
        }
        
        if (getIsDashedLine()) {
            geometryCache->renderVertices(batch, gpu::LINES, _lineVerticesID);
        } else {
            geometryCache->renderVertices(batch, gpu::LINE_STRIP, _lineVerticesID);
        }
    }
    
    // draw our tick marks
    // for our overlay, is solid means we draw a ring between the inner and outer radius of the circle, otherwise
    // we just draw a line...
    if (getHasTickMarks()) {
        if (!_majorTicksVerticesID) {
            _majorTicksVerticesID = geometryCache->allocateID();
        }
        if (!_minorTicksVerticesID) {
            _minorTicksVerticesID = geometryCache->allocateID();
        }
        
        if (geometryChanged) {
            QVector<glm::vec2> majorPoints;
            QVector<glm::vec2> minorPoints;
            
            // draw our major tick marks
            if (getMajorTickMarksAngle() > 0.0f && getMajorTickMarksLength() != 0.0f) {
                
                float tickMarkAngle = getMajorTickMarksAngle();
                float angle = _startAt - fmodf(_startAt, tickMarkAngle) + tickMarkAngle;
                float tickMarkLength = getMajorTickMarksLength();
                float startRadius = (tickMarkLength > 0.0f) ? _innerRadius : _outerRadius;
                float endRadius = startRadius + tickMarkLength;
                
                while (angle <= _endAt) {
                    float angleInRadians = glm::radians(angle);
                    
                    glm::vec2 thisPointA(cosf(angleInRadians) * startRadius, sinf(angleInRadians) * startRadius);
                    glm::vec2 thisPointB(cosf(angleInRadians) * endRadius, sinf(angleInRadians) * endRadius);
                    
                    majorPoints << thisPointA << thisPointB;
                    
                    angle += tickMarkAngle;
                }
            }
            
            // draw our minor tick marks
            if (getMinorTickMarksAngle() > 0.0f && getMinorTickMarksLength() != 0.0f) {
                
                float tickMarkAngle = getMinorTickMarksAngle();
                float angle = _startAt - fmodf(_startAt, tickMarkAngle) + tickMarkAngle;
                float tickMarkLength = getMinorTickMarksLength();
                float startRadius = (tickMarkLength > 0.0f) ? _innerRadius : _outerRadius;
                float endRadius = startRadius + tickMarkLength;
                
                while (angle <= _endAt) {
                    float angleInRadians = glm::radians(angle);
                    
                    glm::vec2 thisPointA(cosf(angleInRadians) * startRadius, sinf(angleInRadians) * startRadius);
                    glm::vec2 thisPointB(cosf(angleInRadians) * endRadius, sinf(angleInRadians) * endRadius);
                    
                    minorPoints << thisPointA << thisPointB;
                    
                    angle += tickMarkAngle;
                }
            }

            glm::vec4 majorColor(toGlm(getMajorTickMarksColor()), alpha);
            geometryCache->updateVertices(_majorTicksVerticesID, majorPoints, majorColor);
            glm::vec4 minorColor(toGlm(getMinorTickMarksColor()), alpha);
            geometryCache->updateVertices(_minorTicksVerticesID, minorPoints, minorColor);
        }

        geometryCache->renderVertices(batch, gpu::LINES, _majorTicksVerticesID);

        geometryCache->renderVertices(batch, gpu::LINES, _minorTicksVerticesID);
    }
}

const render::ShapeKey Circle3DOverlay::getShapeKey() {
    auto builder = render::ShapeKey::Builder().withoutCullFace();
    if (isTransparent()) {
        builder.withTranslucent();
    }
    if (!getIsSolid()) {
        builder.withUnlit().withDepthBias();
    }
    return builder.build();
}

template<typename T> T fromVariant(const QVariant& v, bool& valid) {
    valid = v.isValid();
    return qvariant_cast<T>(v);
}

template<> glm::u8vec3 fromVariant(const QVariant& v, bool& valid) {
    return u8vec3FromVariant(v, valid);
}

template<typename T>
bool updateIfValid(const QVariantMap& properties, const char* key, T& output) {
    bool valid;
    T result = fromVariant<T>(properties[key], valid);
    if (!valid) {
        return false;
    }

    // Don't signal updates if the value was already set
    if (result == output) {
        return false;
    }

    output = result;
    return true;
}

// Multicast, many outputs
template<typename T>
bool updateIfValid(const QVariantMap& properties, const char* key, std::initializer_list<std::reference_wrapper<T>> outputs) {
    bool valid;
    T value = fromVariant<T>(properties[key], valid);
    if (!valid) {
        return false;
    }
    bool updated = false;
    for (T& output : outputs) {
        if (output != value) {
            output = value;
            updated = true;
        }
    }
    return updated;
}

// Multicast, multiple possible inputs, in order of preference
template<typename T>
bool updateIfValid(const QVariantMap& properties, const std::initializer_list<const char*> keys, T& output) {
    for (const char* key : keys) {
        if (updateIfValid<T>(properties, key, output)) {
            return true;
        }
    }
    return false;
}


void Circle3DOverlay::setProperties(const QVariantMap& properties) {
    Planar3DOverlay::setProperties(properties);
    _dirty |= updateIfValid<float>(properties, "alpha", { _innerStartAlpha, _innerEndAlpha, _outerStartAlpha, _outerEndAlpha });
    _dirty |= updateIfValid<float>(properties, "Alpha", { _innerStartAlpha, _innerEndAlpha, _outerStartAlpha, _outerEndAlpha });
    _dirty |= updateIfValid<float>(properties, "startAlpha", { _innerStartAlpha, _outerStartAlpha });
    _dirty |= updateIfValid<float>(properties, "endAlpha", { _innerEndAlpha, _outerEndAlpha });
    _dirty |= updateIfValid<float>(properties, "innerAlpha", { _innerStartAlpha, _innerEndAlpha });
    _dirty |= updateIfValid<float>(properties, "outerAlpha", { _outerStartAlpha, _outerEndAlpha });
    _dirty |= updateIfValid(properties, "innerStartAlpha", _innerStartAlpha);
    _dirty |= updateIfValid(properties, "innerEndAlpha", _innerEndAlpha);
    _dirty |= updateIfValid(properties, "outerStartAlpha", _outerStartAlpha);
    _dirty |= updateIfValid(properties, "outerEndAlpha", _outerEndAlpha);

    _dirty |= updateIfValid<glm::u8vec3>(properties, "color", { _innerStartColor, _innerEndColor, _outerStartColor, _outerEndColor });
    _dirty |= updateIfValid<glm::u8vec3>(properties, "startColor", { _innerStartColor, _outerStartColor } );
    _dirty |= updateIfValid<glm::u8vec3>(properties, "endColor", { _innerEndColor, _outerEndColor } );
    _dirty |= updateIfValid<glm::u8vec3>(properties, "innerColor", { _innerStartColor, _innerEndColor } );
    _dirty |= updateIfValid<glm::u8vec3>(properties, "outerColor", { _outerStartColor, _outerEndColor } );
    _dirty |= updateIfValid(properties, "innerStartColor", _innerStartColor);
    _dirty |= updateIfValid(properties, "innerEndColor", _innerEndColor);
    _dirty |= updateIfValid(properties, "outerStartColor", _outerStartColor);
    _dirty |= updateIfValid(properties, "outerEndColor", _outerEndColor);

    _dirty |= updateIfValid(properties, "startAt", _startAt);
    _dirty |= updateIfValid(properties, "endAt", _endAt);

    _dirty |= updateIfValid(properties, { "radius", "outerRadius" }, _outerRadius);
    _dirty |= updateIfValid(properties, "innerRadius", _innerRadius);
    _dirty |= updateIfValid(properties, "hasTickMarks", _hasTickMarks);
    _dirty |= updateIfValid(properties, "majorTickMarksAngle", _majorTickMarksAngle);
    _dirty |= updateIfValid(properties, "minorTickMarksAngle", _minorTickMarksAngle);
    _dirty |= updateIfValid(properties, "majorTickMarksLength", _majorTickMarksLength);
    _dirty |= updateIfValid(properties, "minorTickMarksLength", _minorTickMarksLength);
    _dirty |= updateIfValid(properties, "majorTickMarksColor", _majorTickMarksColor);
    _dirty |= updateIfValid(properties, "minorTickMarksColor", _minorTickMarksColor);

    if (_innerStartAlpha < 1.0f || _innerEndAlpha < 1.0f || _outerStartAlpha < 1.0f || _outerEndAlpha < 1.0f) {
        // Force the alpha to 0.5, since we'll ignore it in the presence of these other values, but we need
        // it to be non-1 in order to get the right pipeline and non-0 in order to render at all.
        _alpha = 0.5f;
    }
}

// Overlay's color and alpha properties are overridden. And the dimensions property is not used.
/**jsdoc
 * These are the properties of a <code>circle3d</code> {@link Overlays.OverlayType|OverlayType}.
 * @typedef {object} Overlays.Circle3DProperties
 *
 * @property {string} type=circle3d - Has the value <code>"circle3d"</code>. <em>Read-only.</em>
 * @property {number} pulseMax=0 - The maximum value of the pulse multiplier.
 * @property {number} pulseMin=0 - The minimum value of the pulse multiplier.
 * @property {number} pulsePeriod=1 - The duration of the color and alpha pulse, in seconds. A pulse multiplier value goes from
 *     <code>pulseMin</code> to <code>pulseMax</code>, then <code>pulseMax</code> to <code>pulseMin</code> in one period.
 * @property {number} alphaPulse=0 - If non-zero, the alpha of the overlay is pulsed: the alpha value is multiplied by the
 *     current pulse multiplier value each frame. If > 0 the pulse multiplier is applied in phase with the pulse period; if < 0
 *     the pulse multiplier is applied out of phase with the pulse period. (The magnitude of the property isn't otherwise
 *     used.)
 * @property {number} colorPulse=0 - If non-zero, the color of the overlay is pulsed: the color value is multiplied by the
 *     current pulse multiplier value each frame. If > 0 the pulse multiplier is applied in phase with the pulse period; if < 0
 *     the pulse multiplier is applied out of phase with the pulse period. (The magnitude of the property isn't otherwise
 *     used.)
 * @property {boolean} visible=true - If <code>true</code>, the overlay is rendered, otherwise it is not rendered.
 *
 * @property {string} name="" - A friendly name for the overlay.
 * @property {Vec3} position - The position of the overlay center. Synonyms: <code>p1</code>, <code>point</code>, and
 *     <code>start</code>.
 * @property {Vec3} localPosition - The local position of the overlay relative to its parent if the overlay has a
 *     <code>parentID</code> set, otherwise the same value as <code>position</code>.
 * @property {Quat} rotation - The orientation of the overlay. Synonym: <code>orientation</code>.
 * @property {Quat} localRotation - The orientation of the overlay relative to its parent if the overlay has a
 *     <code>parentID</code> set, otherwise the same value as <code>rotation</code>.
 * @property {boolean} isSolid=false - Synonyms: <ode>solid</code>, <code>isFilled</code>, and <code>filled</code>
 *     Antonyms: <code>isWire</code> and <code>wire</code>.
 * @property {boolean} isDashedLine=false - If <code>true</code>, a dashed line is drawn on the overlay's edges. Synonym:
 *     <code>dashed</code>.  Deprecated.
 * @property {boolean} ignorePickIntersection=false - If <code>true</code>, picks ignore the overlay.  <code>ignoreRayIntersection</code> is a synonym.
 * @property {boolean} drawInFront=false - If <code>true</code>, the overlay is rendered in front of other overlays that don't
 *     have <code>drawInFront</code> set to <code>true</code>, and in front of entities.
 * @property {boolean} grabbable=false - Signal to grabbing scripts whether or not this overlay can be grabbed.
 * @property {Uuid} parentID=null - The avatar, entity, or overlay that the overlay is parented to.
 * @property {number} parentJointIndex=65535 - Integer value specifying the skeleton joint that the overlay is attached to if
 *     <code>parentID</code> is an avatar skeleton. A value of <code>65535</code> means "no joint".
 *
 * @property {Vec2} dimensions=1,1 - The dimensions of the overlay. Synonyms: <code>scale</code>, <code>size</code>.
 *     <em>Not used.</em>
 *
 * @property {number} startAt=0 - The counter-clockwise angle from the overlay's x-axis that drawing starts at, in degrees.
 * @property {number} endAt=360 - The counter-clockwise angle from the overlay's x-axis that drawing ends at, in degrees.
 * @property {number} outerRadius=1 - The outer radius of the overlay, in meters. Synonym: <code>radius</code>.
 * @property {number} innerRadius=0 - The inner radius of the overlay, in meters.
  * @property {Color} color=255,255,255 - The color of the overlay. Setting this value also sets the values of
 *     <code>innerStartColor</code>, <code>innerEndColor</code>, <code>outerStartColor</code>, and <code>outerEndColor</code>.
 * @property {Color} startColor - Sets the values of <code>innerStartColor</code> and <code>outerStartColor</code>.
 *     <em>Write-only.</em>
 * @property {Color} endColor - Sets the values of <code>innerEndColor</code> and <code>outerEndColor</code>.
 *     <em>Write-only.</em>
 * @property {Color} innerColor - Sets the values of <code>innerStartColor</code> and <code>innerEndColor</code>.
 *     <em>Write-only.</em>
 * @property {Color} outerColor - Sets the values of <code>outerStartColor</code> and <code>outerEndColor</code>.
 *     <em>Write-only.</em>
 * @property {Color} innerStartcolor - The color at the inner start point of the overlay.
 * @property {Color} innerEndColor - The color at the inner end point of the overlay.
 * @property {Color} outerStartColor - The color at the outer start point of the overlay.
 * @property {Color} outerEndColor - The color at the outer end point of the overlay.
 * @property {number} alpha=0.5 - The opacity of the overlay, <code>0.0</code> - <code>1.0</code>. Setting this value also sets
 *     the values of <code>innerStartAlpha</code>, <code>innerEndAlpha</code>, <code>outerStartAlpha</code>, and 
 *     <code>outerEndAlpha</code>. Synonym: <code>Alpha</code>; <em>write-only</em>.
 * @property {number} startAlpha - Sets the values of <code>innerStartAlpha</code> and <code>outerStartAlpha</code>.
 *     <em>Write-only.</em>
 * @property {number} endAlpha - Sets the values of <code>innerEndAlpha</code> and <code>outerEndAlpha</code>.
 *     <em>Write-only.</em>
 * @property {number} innerAlpha - Sets the values of <code>innerStartAlpha</code> and <code>innerEndAlpha</code>.
 *     <em>Write-only.</em>
 * @property {number} outerAlpha - Sets the values of <code>outerStartAlpha</code> and <code>outerEndAlpha</code>.
 *     <em>Write-only.</em>
 * @property {number} innerStartAlpha=0 - The alpha at the inner start point of the overlay.
 * @property {number} innerEndAlpha=0 - The alpha at the inner end point of the overlay.
 * @property {number} outerStartAlpha=0 - The alpha at the outer start point of the overlay.
 * @property {number} outerEndAlpha=0 - The alpha at the outer end point of the overlay.

 * @property {boolean} hasTickMarks=false - If <code>true</code>, tick marks are drawn.
 * @property {number} majorTickMarksAngle=0 - The angle between major tick marks, in degrees.
 * @property {number} minorTickMarksAngle=0 - The angle between minor tick marks, in degrees.
 * @property {number} majorTickMarksLength=0 - The length of the major tick marks, in meters. A positive value draws tick marks
 *     outwards from the inner radius; a negative value draws tick marks inwards from the outer radius.
 * @property {number} minorTickMarksLength=0 - The length of the minor tick marks, in meters. A positive value draws tick marks
 *     outwards from the inner radius; a negative value draws tick marks inwards from the outer radius.
 * @property {Color} majorTickMarksColor=0,0,0 - The color of the major tick marks.
 * @property {Color} minorTickMarksColor=0,0,0 - The color of the minor tick marks.
 */
QVariant Circle3DOverlay::getProperty(const QString& property) {
    if (property == "startAt") {
        return _startAt;
    }
    if (property == "endAt") {
        return _endAt;
    }
    if (property == "radius") {
        return _outerRadius;
    }
    if (property == "outerRadius") {
        return _outerRadius;
    }
    if (property == "innerRadius") {
        return _innerRadius;
    }
    if (property == "innerStartColor") {
        return u8vec3ColortoVariant(_innerStartColor);
    }
    if (property == "innerEndColor") {
        return u8vec3ColortoVariant(_innerEndColor);
    }
    if (property == "outerStartColor") {
        return u8vec3ColortoVariant(_outerStartColor);
    }
    if (property == "outerEndColor") {
        return u8vec3ColortoVariant(_outerEndColor);
    }
    if (property == "innerStartAlpha") {
        return _innerStartAlpha;
    }
    if (property == "innerEndAlpha") {
        return _innerEndAlpha;
    }
    if (property == "outerStartAlpha") {
        return _outerStartAlpha;
    }
    if (property == "outerEndAlpha") {
        return _outerEndAlpha;
    }
    if (property == "hasTickMarks") {
        return _hasTickMarks;
    }
    if (property == "majorTickMarksAngle") {
        return _majorTickMarksAngle;
    }
    if (property == "minorTickMarksAngle") {
        return _minorTickMarksAngle;
    }
    if (property == "majorTickMarksLength") {
        return _majorTickMarksLength;
    }
    if (property == "minorTickMarksLength") {
        return _minorTickMarksLength;
    }
    if (property == "majorTickMarksColor") {
        return u8vec3ColortoVariant(_majorTickMarksColor);
    }
    if (property == "minorTickMarksColor") {
        return u8vec3ColortoVariant(_minorTickMarksColor);
    }

    return Planar3DOverlay::getProperty(property);
}

bool Circle3DOverlay::findRayIntersection(const glm::vec3& origin, const glm::vec3& direction, float& distance, 
                                          BoxFace& face, glm::vec3& surfaceNormal, bool precisionPicking) {
    // Scale the dimensions by the diameter
    glm::vec2 dimensions = getOuterRadius() * 2.0f * getDimensions();
    glm::quat rotation = getWorldOrientation();

    if (findRayRectangleIntersection(origin, direction, rotation, getWorldPosition(), dimensions, distance)) {
        glm::vec3 hitPosition = origin + (distance * direction);
        glm::vec3 localHitPosition = glm::inverse(getWorldOrientation()) * (hitPosition - getWorldPosition());
        localHitPosition.x /= getDimensions().x;
        localHitPosition.y /= getDimensions().y;
        float distanceToHit = glm::length(localHitPosition);

        if (getInnerRadius() <= distanceToHit && distanceToHit <= getOuterRadius()) {
            glm::vec3 forward = rotation * Vectors::FRONT;
            if (glm::dot(forward, direction) > 0.0f) {
                face = MAX_Z_FACE;
                surfaceNormal = -forward;
            } else {
                face = MIN_Z_FACE;
                surfaceNormal = forward;
            }
            return true;
        }
    }

    return false;
}

bool Circle3DOverlay::findParabolaIntersection(const glm::vec3& origin, const glm::vec3& velocity, const glm::vec3& acceleration,
                                               float& parabolicDistance, BoxFace& face, glm::vec3& surfaceNormal, bool precisionPicking) {
    // Scale the dimensions by the diameter
    glm::vec2 xyDimensions = getOuterRadius() * 2.0f * getDimensions();
    glm::quat rotation = getWorldOrientation();
    glm::vec3 position = getWorldPosition();

    glm::quat inverseRot = glm::inverse(rotation);
    glm::vec3 localOrigin = inverseRot * (origin - position);
    glm::vec3 localVelocity = inverseRot * velocity;
    glm::vec3 localAcceleration = inverseRot * acceleration;

    if (findParabolaRectangleIntersection(localOrigin, localVelocity, localAcceleration, xyDimensions, parabolicDistance)) {
        glm::vec3 localHitPosition = localOrigin + localVelocity * parabolicDistance + 0.5f * localAcceleration * parabolicDistance * parabolicDistance;
        localHitPosition.x /= getDimensions().x;
        localHitPosition.y /= getDimensions().y;
        float distanceToHit = glm::length(localHitPosition);

        if (getInnerRadius() <= distanceToHit && distanceToHit <= getOuterRadius()) {
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
        }
    }

    return false;
}

Circle3DOverlay* Circle3DOverlay::createClone() const {
    return new Circle3DOverlay(this);
}
