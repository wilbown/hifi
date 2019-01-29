//
//  ModelOverlay.cpp
//
//
//  Created by Clement on 6/30/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "ModelOverlay.h"

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

#include <Rig.h>

#include "Application.h"


QString const ModelOverlay::TYPE = "model";

ModelOverlay::ModelOverlay()
    : _model(std::make_shared<Model>(nullptr, this)),
      _modelTextures(QVariantMap())
{
    _model->setLoadingPriority(_loadPriority);
    _isLoaded = false;
    render::ScenePointer scene = qApp->getMain3DScene();
    _model->setVisibleInScene(false, scene);
}

ModelOverlay::ModelOverlay(const ModelOverlay* modelOverlay) :
    Volume3DOverlay(modelOverlay),
    _model(std::make_shared<Model>(nullptr, this)),
    _modelTextures(QVariantMap()),
    _url(modelOverlay->_url),
    _updateModel(false),
    _scaleToFit(modelOverlay->_scaleToFit),
    _loadPriority(modelOverlay->_loadPriority),

    _animationURL(modelOverlay->_animationURL),
    _animationFPS(modelOverlay->_animationFPS),
    _animationCurrentFrame(modelOverlay->_animationCurrentFrame),
    _animationRunning(modelOverlay->_animationRunning),
    _animationLoop(modelOverlay->_animationLoop),
    _animationFirstFrame(modelOverlay->_animationFirstFrame),
    _animationLastFrame(modelOverlay->_animationLastFrame),
    _animationHold(modelOverlay->_animationHold),
    _animationAllowTranslation(modelOverlay->_animationAllowTranslation)

    // Joint translations and rotations aren't copied because the model needs to load before they can be applied.
{
    _model->setLoadingPriority(_loadPriority);
    if (_url.isValid()) {
        _updateModel = true;
        _isLoaded = false;
    }
}

void ModelOverlay::update(float deltatime) {
    Base3DOverlay::update(deltatime);

    if (_updateModel) {
        _updateModel = false;
        _model->setSnapModelToCenter(true);
        Transform transform = evalRenderTransform();
        if (_scaleToFit) {
            _model->setScaleToFit(true, transform.getScale() * getDimensions());
        } else {
            _model->setScale(transform.getScale());
        }
        _model->setRotation(transform.getRotation());
        _model->setTranslation(transform.getTranslation());
        _model->setURL(_url);
        _model->simulate(deltatime, true);
    } else {
        _model->simulate(deltatime);
    }
    _isLoaded = _model->isActive();

    if (isAnimatingSomething()) {
        if (!jointsMapped()) {
            mapAnimationJoints(_model->getJointNames());
        }
        animate();
    }

    // check to see if when we added our model to the scene they were ready, if they were not ready, then
    // fix them up in the scene
    render::ScenePointer scene = qApp->getMain3DScene();
    render::Transaction transaction;
    if (_model->needsFixupInScene()) {
        emit DependencyManager::get<scriptable::ModelProviderFactory>()->modelRemovedFromScene(getID(), NestableType::Overlay, _model);
        _model->removeFromScene(scene, transaction);
        _model->addToScene(scene, transaction);

        auto newRenderItemIDs{ _model->fetchRenderItemIDs() };
        transaction.updateItem<Overlay>(getRenderItemID(), [newRenderItemIDs](Overlay& data) {
            auto modelOverlay = static_cast<ModelOverlay*>(&data);
            modelOverlay->setSubRenderItemIDs(newRenderItemIDs);
        });
        processMaterials();
        emit DependencyManager::get<scriptable::ModelProviderFactory>()->modelAddedToScene(getID(), NestableType::Overlay, _model);
    }
    bool metaDirty = false;
    if (_visibleDirty && _texturesLoaded) {
        _visibleDirty = false;
        // don't show overlays in mirrors or spectator-cam unless _isVisibleInSecondaryCamera is true
        uint8_t modelRenderTagMask = (_isVisibleInSecondaryCamera ? render::hifi::TAG_ALL_VIEWS : render::hifi::TAG_MAIN_VIEW);

        _model->setTagMask(modelRenderTagMask, scene);
        _model->setVisibleInScene(getVisible(), scene);
        metaDirty = true;
    }
    if (_drawInFrontDirty) {
        _drawInFrontDirty = false;
        _model->setLayeredInFront(getDrawInFront(), scene);
        metaDirty = true;
    }
    if (_drawInHUDDirty) {
        _drawInHUDDirty = false;
        _model->setLayeredInHUD(getDrawHUDLayer(), scene);
        metaDirty = true;
    }
    if (_groupCulledDirty) {
        _groupCulledDirty = false;
        _model->setGroupCulled(_isGroupCulled, scene);
        metaDirty = true;
    }
    if (metaDirty) {
        transaction.updateItem<Overlay>(getRenderItemID(), [](Overlay& data) {});
    }
    scene->enqueueTransaction(transaction);

    if (_texturesDirty && !_modelTextures.isEmpty()) {
        _texturesDirty = false;
        _model->setTextures(_modelTextures);
    }

    if (!_texturesLoaded && _model->getGeometry() && _model->getGeometry()->areTexturesLoaded()) {
        _texturesLoaded = true;

        _model->setVisibleInScene(getVisible(), scene);
        _model->updateRenderItems();
    }
}

bool ModelOverlay::addToScene(Overlay::Pointer overlay, const render::ScenePointer& scene, render::Transaction& transaction) {
    Volume3DOverlay::addToScene(overlay, scene, transaction);
    _model->addToScene(scene, transaction);
    processMaterials();
    emit DependencyManager::get<scriptable::ModelProviderFactory>()->modelAddedToScene(getID(), NestableType::Overlay, _model);
    return true;
}

void ModelOverlay::removeFromScene(Overlay::Pointer overlay, const render::ScenePointer& scene, render::Transaction& transaction) {
    Volume3DOverlay::removeFromScene(overlay, scene, transaction);
    _model->removeFromScene(scene, transaction);
    emit DependencyManager::get<scriptable::ModelProviderFactory>()->modelRemovedFromScene(getID(), NestableType::Overlay, _model);
    transaction.updateItem<Overlay>(getRenderItemID(), [](Overlay& data) {
        auto modelOverlay = static_cast<ModelOverlay*>(&data);
        modelOverlay->clearSubRenderItemIDs();
    });
}

void ModelOverlay::setVisible(bool visible) {
    if (visible != getVisible()) {
        Overlay::setVisible(visible);
        _visibleDirty = true;
    }
}

void ModelOverlay::setDrawInFront(bool drawInFront) {
    if (drawInFront != getDrawInFront()) {
        Base3DOverlay::setDrawInFront(drawInFront);
        _drawInFrontDirty = true;
    }
}

void ModelOverlay::setDrawHUDLayer(bool drawHUDLayer) {
    if (drawHUDLayer != getDrawHUDLayer()) {
        Base3DOverlay::setDrawHUDLayer(drawHUDLayer);
        _drawInHUDDirty = true;
    }
}

void ModelOverlay::setGroupCulled(bool groupCulled) {
    if (groupCulled != _isGroupCulled) {
        _isGroupCulled = groupCulled;
        _groupCulledDirty = true;
    }
}

void ModelOverlay::setProperties(const QVariantMap& properties) {
    auto origPosition = getWorldPosition();
    auto origRotation = getWorldOrientation();
    auto origDimensions = getDimensions();
    auto origScale = getSNScale();

    Base3DOverlay::setProperties(properties);

    auto scale = properties["scale"];
    if (scale.isValid()) {
        setSNScale(vec3FromVariant(scale));
    }

    auto dimensions = properties["dimensions"];
    if (!dimensions.isValid()) {
        dimensions = properties["size"];
    }
    if (dimensions.isValid()) {
        _scaleToFit = true;
        setDimensions(vec3FromVariant(dimensions));
    } else if (scale.isValid()) {
        // if "scale" property is set but "dimensions" is not.
        // do NOT scale to fit.
        _scaleToFit = false;
    }

    if (origPosition != getWorldPosition() || origRotation != getWorldOrientation() || origDimensions != getDimensions() || origScale != getSNScale()) {
        _updateModel = true;
    }

    auto loadPriorityProperty = properties["loadPriority"];
    if (loadPriorityProperty.isValid()) {
        _loadPriority = loadPriorityProperty.toFloat();
        _model->setLoadingPriority(_loadPriority);
    }

    auto urlValue = properties["url"];
    if (urlValue.isValid() && urlValue.canConvert<QString>()) {
        _url = urlValue.toString();
        _updateModel = true;
        _isLoaded = false;
        _texturesLoaded = false;
    }

    auto texturesValue = properties["textures"];
    if (texturesValue.isValid() && texturesValue.canConvert(QVariant::Map)) {
        _texturesLoaded = false;
        QVariantMap textureMap = texturesValue.toMap();
        _modelTextures = textureMap;
        _texturesDirty = true;
    }

    auto groupCulledValue = properties["isGroupCulled"];
    if (groupCulledValue.isValid() && groupCulledValue.canConvert(QVariant::Bool)) {
        setGroupCulled(groupCulledValue.toBool());
    }

    // jointNames is read-only.
    // jointPositions is read-only.
    // jointOrientations is read-only.

    // relative
    auto jointTranslationsValue = properties["jointTranslations"];
    if (jointTranslationsValue.canConvert(QVariant::List)) {
        const QVariantList& jointTranslations = jointTranslationsValue.toList();
        int translationCount = jointTranslations.size();
        int jointCount = _model->getJointStateCount();
        if (translationCount < jointCount) {
            jointCount = translationCount;
        }
        for (int i=0; i < jointCount; i++) {
            const auto& translationValue = jointTranslations[i];
            if (translationValue.isValid()) {
                _model->setJointTranslation(i, true, vec3FromVariant(translationValue), 1.0f);
            }
        }
        _updateModel = true;
    }

    // relative
    auto jointRotationsValue = properties["jointRotations"];
    if (jointRotationsValue.canConvert(QVariant::List)) {
        const QVariantList& jointRotations = jointRotationsValue.toList();
        int rotationCount = jointRotations.size();
        int jointCount = _model->getJointStateCount();
        if (rotationCount < jointCount) {
            jointCount = rotationCount;
        }
        for (int i=0; i < jointCount; i++) {
            const auto& rotationValue = jointRotations[i];
            if (rotationValue.isValid()) {
                _model->setJointRotation(i, true, quatFromVariant(rotationValue), 1.0f);
            }
        }
        _updateModel = true;
    }

    auto animationSettings = properties["animationSettings"];
    if (animationSettings.canConvert(QVariant::Map)) {
        QVariantMap animationSettingsMap = animationSettings.toMap();

        auto animationURL = animationSettingsMap["url"];
        auto animationFPS = animationSettingsMap["fps"];
        auto animationCurrentFrame = animationSettingsMap["currentFrame"];
        auto animationFirstFrame = animationSettingsMap["firstFrame"];
        auto animationLastFrame = animationSettingsMap["lastFrame"];
        auto animationRunning = animationSettingsMap["running"];
        auto animationLoop = animationSettingsMap["loop"];
        auto animationHold = animationSettingsMap["hold"];
        auto animationAllowTranslation = animationSettingsMap["allowTranslation"];

        if (animationURL.canConvert(QVariant::Url)) {
            _animationURL = animationURL.toUrl();
        }
        if (animationFPS.isValid()) {
            _animationFPS = animationFPS.toFloat();
        }
        if (animationCurrentFrame.isValid()) {
            _animationCurrentFrame = animationCurrentFrame.toFloat();
        }
        if (animationFirstFrame.isValid()) {
            _animationFirstFrame = animationFirstFrame.toFloat();
        }
        if (animationLastFrame.isValid()) {
            _animationLastFrame = animationLastFrame.toFloat();
        }

        if (animationRunning.canConvert(QVariant::Bool)) {
            _animationRunning = animationRunning.toBool();
        }
        if (animationLoop.canConvert(QVariant::Bool)) {
            _animationLoop = animationLoop.toBool();
        }
        if (animationHold.canConvert(QVariant::Bool)) {
            _animationHold = animationHold.toBool();
        }
        if (animationAllowTranslation.canConvert(QVariant::Bool)) {
            _animationAllowTranslation = animationAllowTranslation.toBool();
        }

    }
}

template <typename vectorType, typename itemType>
vectorType ModelOverlay::mapJoints(mapFunction<itemType> function) const {
    vectorType result;
    if (_model && _model->isActive()) {
        const int jointCount = _model->getJointStateCount();
        result.reserve(jointCount);
        for (int i = 0; i < jointCount; i++) {
            result << function(i);
        }
    }
    return result;
}

// Note: ModelOverlay overrides Volume3DOverlay's "dimensions" and "scale" properties.
/**jsdoc
 * These are the properties of a <code>model</code> {@link Overlays.OverlayType|OverlayType}.
 * @typedef {object} Overlays.ModelProperties
 *
 * @property {string} type=sphere - Has the value <code>"model"</code>. <em>Read-only.</em>
 * @property {Color} color=255,255,255 - The color of the overlay.
 * @property {number} alpha=0.7 - The opacity of the overlay, <code>0.0</code> - <code>1.0</code>.
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
 * @property {boolean} isSolid=false - Synonyms: <ode>solid</code>, <code>isFilled</code>, and <code>filled</code>.
 *     Antonyms: <code>isWire</code> and <code>wire</code>.
 * @property {boolean} isDashedLine=false - If <code>true</code>, a dashed line is drawn on the overlay's edges. Synonym:
 *     <code>dashed</code>.  Deprecated.
 * @property {boolean} ignorePickIntersection=false - If <code>true</code>, picks ignore the overlay.  <code>ignoreRayIntersection</code> is a synonym.
 * @property {boolean} drawInFront=false - If <code>true</code>, the overlay is rendered in front of other overlays that don't
 *     have <code>drawInFront</code> set to <code>true</code>, and in front of entities.
 * @property {boolean} isGroupCulled=false - If <code>true</code>, the mesh parts of the model are LOD culled as a group.
 *     If <code>false</code>, separate mesh parts will be LOD culled individually.
 * @property {boolean} grabbable=false - Signal to grabbing scripts whether or not this overlay can be grabbed.
 * @property {Uuid} parentID=null - The avatar, entity, or overlay that the overlay is parented to.
 * @property {number} parentJointIndex=65535 - Integer value specifying the skeleton joint that the overlay is attached to if
 *     <code>parentID</code> is an avatar skeleton. A value of <code>65535</code> means "no joint".
 *
 * @property {string} url - The URL of the FBX or OBJ model used for the overlay.
 * @property {number} loadPriority=0.0 - The priority for loading and displaying the overlay. Overlays with higher values load 
 *     first.
 * @property {Vec3} dimensions - The dimensions of the overlay. Synonym: <code>size</code>.
 * @property {Vec3} scale - The scale factor applied to the model's dimensions.
 * @property {object.<name, url>} textures - Maps the named textures in the model to the JPG or PNG images in the urls.
 * @property {Array.<string>} jointNames - The names of the joints - if any - in the model. <em>Read-only.</em>
 * @property {Array.<Quat>} jointRotations - The relative rotations of the model's joints. <em>Not copied if overlay is 
 *     cloned.</em>
 * @property {Array.<Vec3>} jointTranslations - The relative translations of the model's joints. <em>Not copied if overlay is 
 *     cloned.</em>
 * @property {Array.<Quat>} jointOrientations - The absolute orientations of the model's joints, in world coordinates.
 *     <em>Read-only.</em>
 * @property {Array.<Vec3>} jointPositions - The absolute positions of the model's joints, in world coordinates.
 *     <em>Read-only.</em>
 * @property {string} animationSettings.url="" - The URL of an FBX file containing an animation to play.
 * @property {number} animationSettings.fps=0 - The frame rate (frames/sec) to play the animation at. 
 * @property {number} animationSettings.firstFrame=0 - The frame to start playing at.
 * @property {number} animationSettings.lastFrame=0 - The frame to finish playing at.
 * @property {number} animationSettings.currentFrame=0 - The current frame being played.
 * @property {boolean} animationSettings.running=false - Whether or not the animation is playing.
 * @property {boolean} animationSettings.loop=false - Whether or not the animation should repeat in a loop.
 * @property {boolean} animationSettings.hold=false - Whether or not when the animation finishes, the rotations and 
 *     translations of the last frame played should be maintained.
 * @property {boolean} animationSettings.allowTranslation=false - Whether or not translations contained in the animation should
 *     be played.
 */
QVariant ModelOverlay::getProperty(const QString& property) {
    if (property == "url") {
        return _url.toString();
    }
    if (property == "dimensions" || property == "size") {
        return vec3toVariant(getDimensions());
    }
    if (property == "scale") {
        return vec3toVariant(getSNScale());
    }
    if (property == "textures") {
        if (_modelTextures.size() > 0) {
            QVariantMap textures;
            foreach(const QString& key, _modelTextures.keys()) {
                textures[key] = _modelTextures[key].toString();
            }
            return textures;
        } else {
            return QVariant();
        }
    }

    if (property == "loadPriority") {
        return _loadPriority;
    }

    if (property == "jointNames") {
        if (_model && _model->isActive()) {
            // note: going through Rig because Model::getJointNames() (which proxies to HFMModel) was always empty
            const Rig* rig = &(_model->getRig());
            return mapJoints<QStringList, QString>([rig](int jointIndex) -> QString {
                return rig->nameOfJoint(jointIndex);
            });
        }
    }

    // relative
    if (property == "jointRotations") {
        return mapJoints<QVariantList, QVariant>(
            [this](int jointIndex) -> QVariant {
                glm::quat rotation;
                _model->getJointRotation(jointIndex, rotation);
                return quatToVariant(rotation);
            });
    }

    // relative
    if (property == "jointTranslations") {
        return mapJoints<QVariantList, QVariant>(
            [this](int jointIndex) -> QVariant {
                glm::vec3 translation;
                _model->getJointTranslation(jointIndex, translation);
                return vec3toVariant(translation);
            });
    }

    // absolute
    if (property == "jointOrientations") {
        return mapJoints<QVariantList, QVariant>(
            [this](int jointIndex) -> QVariant {
                glm::quat orientation;
                _model->getJointRotationInWorldFrame(jointIndex, orientation);
                return quatToVariant(orientation);
            });
    }

    // absolute
    if (property == "jointPositions") {
        return mapJoints<QVariantList, QVariant>(
            [this](int jointIndex) -> QVariant {
                glm::vec3 position;
                _model->getJointPositionInWorldFrame(jointIndex, position);
                return vec3toVariant(position);
            });
    }

    // animation properties
    if (property == "animationSettings") {
        QVariantMap animationSettingsMap;

        animationSettingsMap["url"] = _animationURL;
        animationSettingsMap["fps"] = _animationFPS;
        animationSettingsMap["currentFrame"] = _animationCurrentFrame;
        animationSettingsMap["firstFrame"] = _animationFirstFrame;
        animationSettingsMap["lastFrame"] = _animationLastFrame;
        animationSettingsMap["running"] = _animationRunning;
        animationSettingsMap["loop"] = _animationLoop;
        animationSettingsMap["hold"]= _animationHold;
        animationSettingsMap["allowTranslation"] = _animationAllowTranslation;

        return animationSettingsMap;
    }


    return Volume3DOverlay::getProperty(property);
}

bool ModelOverlay::findRayIntersection(const glm::vec3& origin, const glm::vec3& direction,
                                       float& distance, BoxFace& face, glm::vec3& surfaceNormal, bool precisionPicking) {
    QVariantMap extraInfo;
    return _model->findRayIntersectionAgainstSubMeshes(origin, direction, distance, face, surfaceNormal, extraInfo, precisionPicking);
}

bool ModelOverlay::findRayIntersectionExtraInfo(const glm::vec3& origin, const glm::vec3& direction,
                                                float& distance, BoxFace& face, glm::vec3& surfaceNormal, QVariantMap& extraInfo, bool precisionPicking) {
    return _model->findRayIntersectionAgainstSubMeshes(origin, direction, distance, face, surfaceNormal, extraInfo, precisionPicking);
}

bool ModelOverlay::findParabolaIntersection(const glm::vec3& origin, const glm::vec3& velocity, const glm::vec3& acceleration,
                                            float& parabolicDistance, BoxFace& face, glm::vec3& surfaceNormal, bool precisionPicking) {
    QVariantMap extraInfo;
    return _model->findParabolaIntersectionAgainstSubMeshes(origin, velocity, acceleration, parabolicDistance, face, surfaceNormal, extraInfo, precisionPicking);
}

bool ModelOverlay::findParabolaIntersectionExtraInfo(const glm::vec3& origin, const glm::vec3& velocity, const glm::vec3& acceleration,
                                                     float& parabolicDistance, BoxFace& face, glm::vec3& surfaceNormal, QVariantMap& extraInfo, bool precisionPicking) {
    return _model->findParabolaIntersectionAgainstSubMeshes(origin, velocity, acceleration, parabolicDistance, face, surfaceNormal, extraInfo, precisionPicking);
}

ModelOverlay* ModelOverlay::createClone() const {
    return new ModelOverlay(this);
}

Transform ModelOverlay::evalRenderTransform() {
    Transform transform = getTransform();
    transform.setScale(1.0f); // disable inherited scale
    return transform;
}

void ModelOverlay::locationChanged(bool tellPhysics) {
    Base3DOverlay::locationChanged(tellPhysics);

    // FIXME Start using the _renderTransform instead of calling for Transform and Dimensions from here, do the custom things needed in evalRenderTransform()
    if (_model && _model->isActive()) {
        _model->setRotation(getWorldOrientation());
        _model->setTranslation(getWorldPosition());
        _updateModel = true;
    }
}

QString ModelOverlay::getName() const {
    if (_name != "") {
        return QString("Overlay:") + getType() + ":" + _name;
    }
    return QString("Overlay:") + getType() + ":" + _url.toString();
}


void ModelOverlay::animate() {

    if (!_animation || !_animation->isLoaded() || !_model || !_model->isLoaded()) {
        return;
    }


    QVector<JointData> jointsData;

    const QVector<HFMAnimationFrame>&  frames = _animation->getFramesReference(); // NOTE: getFrames() is too heavy
    int frameCount = frames.size();
    if (frameCount <= 0) {
        return;
    }

    if (!_lastAnimated) {
        _lastAnimated = usecTimestampNow();
        return;
    }

    auto now = usecTimestampNow();
    auto interval = now - _lastAnimated;
    _lastAnimated = now;
    float deltaTime = (float)interval / (float)USECS_PER_SECOND;
    _animationCurrentFrame += (deltaTime * _animationFPS);

    int animationCurrentFrame = (int)(glm::floor(_animationCurrentFrame)) % frameCount;
    if (animationCurrentFrame < 0 || animationCurrentFrame > frameCount) {
        animationCurrentFrame = 0;
    }

    if (animationCurrentFrame == _lastKnownCurrentFrame) {
        return;
    }
    _lastKnownCurrentFrame = animationCurrentFrame;

    if (_jointMapping.size() != _model->getJointStateCount()) {
        return;
    }

    QStringList animationJointNames = _animation->getHFMModel().getJointNames();
    auto& hfmJoints = _animation->getHFMModel().joints;

    auto& originalHFMJoints = _model->getHFMModel().joints;
    auto& originalHFMIndices = _model->getHFMModel().jointIndices;

    const QVector<glm::quat>& rotations = frames[_lastKnownCurrentFrame].rotations;
    const QVector<glm::vec3>& translations = frames[_lastKnownCurrentFrame].translations;

    jointsData.resize(_jointMapping.size());
    for (int j = 0; j < _jointMapping.size(); j++) {
        int index = _jointMapping[j];

        if (index >= 0) {
            glm::mat4 translationMat;

            if (_animationAllowTranslation) {
                if (index < translations.size()) {
                    translationMat = glm::translate(translations[index]);
                }
            } else if (index < animationJointNames.size()) {
                QString jointName = hfmJoints[index].name;

                if (originalHFMIndices.contains(jointName)) {
                    // Making sure the joint names exist in the original model the animation is trying to apply onto. If they do, then remap and get its translation.
                    int remappedIndex = originalHFMIndices[jointName] - 1; // JointIndeces seem to always start from 1 and the found index is always 1 higher than actual.
                    translationMat = glm::translate(originalHFMJoints[remappedIndex].translation);
                }
            }
            glm::mat4 rotationMat;
            if (index < rotations.size()) {
                rotationMat = glm::mat4_cast(hfmJoints[index].preRotation * rotations[index] * hfmJoints[index].postRotation);
            } else {
                rotationMat = glm::mat4_cast(hfmJoints[index].preRotation * hfmJoints[index].postRotation);
            }

            glm::mat4 finalMat = (translationMat * hfmJoints[index].preTransform *
                rotationMat * hfmJoints[index].postTransform);
            auto& jointData = jointsData[j];
            jointData.translation = extractTranslation(finalMat);
            jointData.translationIsDefaultPose = false;
            jointData.rotation = glmExtractRotation(finalMat);
            jointData.rotationIsDefaultPose = false;
        }
    }
    // Set the data in the model
    copyAnimationJointDataToModel(jointsData);
}


void ModelOverlay::mapAnimationJoints(const QStringList& modelJointNames) {

    // if we don't have animation, or we're already joint mapped then bail early
    if (!hasAnimation() || jointsMapped()) {
        return;
    }

    if (!_animation || _animation->getURL() != _animationURL) {
        _animation = DependencyManager::get<AnimationCache>()->getAnimation(_animationURL);
    }

    if (_animation && _animation->isLoaded()) {
        QStringList animationJointNames = _animation->getJointNames();

        if (modelJointNames.size() > 0 && animationJointNames.size() > 0) {
            _jointMapping.resize(modelJointNames.size());
            for (int i = 0; i < modelJointNames.size(); i++) {
                _jointMapping[i] = animationJointNames.indexOf(modelJointNames[i]);
            }
            _jointMappingCompleted = true;
            _jointMappingURL = _animationURL;
        }
    }
}

void ModelOverlay::copyAnimationJointDataToModel(QVector<JointData> jointsData) {
    if (!_model || !_model->isLoaded()) {
        return;
    }

    // relay any inbound joint changes from scripts/animation/network to the model/rig
    for (int index = 0; index < jointsData.size(); ++index) {
        auto& jointData = jointsData[index];
        _model->setJointRotation(index, true, jointData.rotation, 1.0f);
        _model->setJointTranslation(index, true, jointData.translation, 1.0f);
    }
    _updateModel = true;
}

void ModelOverlay::clearSubRenderItemIDs() {
    _subRenderItemIDs.clear();
}

void ModelOverlay::setSubRenderItemIDs(const render::ItemIDs& ids) {
    _subRenderItemIDs = ids;
}

uint32_t ModelOverlay::fetchMetaSubItems(render::ItemIDs& subItems) const {
    if (_model) {
        auto metaSubItems = _subRenderItemIDs;
        subItems.insert(subItems.end(), metaSubItems.begin(), metaSubItems.end());
        return (uint32_t)metaSubItems.size();
    }
    return 0;
}

void ModelOverlay::addMaterial(graphics::MaterialLayer material, const std::string& parentMaterialName) {
    Overlay::addMaterial(material, parentMaterialName);
    if (_model && _model->fetchRenderItemIDs().size() > 0) {
        _model->addMaterial(material, parentMaterialName);
    }
}

void ModelOverlay::removeMaterial(graphics::MaterialPointer material, const std::string& parentMaterialName) {
    Overlay::removeMaterial(material, parentMaterialName);
    if (_model && _model->fetchRenderItemIDs().size() > 0) {
        _model->removeMaterial(material, parentMaterialName);
    }
}

void ModelOverlay::processMaterials() {
    assert(_model);
    std::lock_guard<std::mutex> lock(_materialsLock);
    for (auto& shapeMaterialPair : _materials) {
        auto material = shapeMaterialPair.second;
        while (!material.empty()) {
            _model->addMaterial(material.top(), shapeMaterialPair.first);
            material.pop();
        }
    }
}

bool ModelOverlay::canReplaceModelMeshPart(int meshIndex, int partIndex) {
    // TODO: bounds checking; for now just used to indicate provider generally supports mesh updates
    return _model && _model->isLoaded();
}

bool ModelOverlay::replaceScriptableModelMeshPart(scriptable::ScriptableModelBasePointer newModel, int meshIndex, int partIndex) {
    return canReplaceModelMeshPart(meshIndex, partIndex) &&
        _model->replaceScriptableModelMeshPart(newModel, meshIndex, partIndex);
}

scriptable::ScriptableModelBase ModelOverlay::getScriptableModel() {
    if (!_model || !_model->isLoaded()) {
        return Base3DOverlay::getScriptableModel();
    }
    auto result = _model->getScriptableModel();
    result.objectID = getID();
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        result.appendMaterials(_materials);
    }
    return result;
}

render::ItemKey ModelOverlay::getKey() {
    auto builder = render::ItemKey::Builder(Base3DOverlay::getKey());
    if (_isGroupCulled) {
        builder.withMetaCullGroup();
    }
    return builder.build();
}
