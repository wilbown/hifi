//
//  Keyboard.h
//  interface/src/scripting
//
//  Created by Dante Ruiz on 2018-08-27.
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_Keyboard_h
#define hifi_Keyboard_h

#include <map>
#include <memory>
#include <vector>

#include <QtCore/QObject>
#include <QTimer>
#include <QHash>
#include <DependencyManager.h>
#include <Sound.h>
#include <AudioInjector.h>
#include <shared/ReadWriteLockable.h>
#include <SettingHandle.h>

#include "ui/overlays/Overlay.h"

class PointerEvent;


class Key {
public:
    Key() = default;
    ~Key() = default;

    enum Type {
        CHARACTER,
        CAPS,
        CLOSE,
        LAYER,
        BACKSPACE,
        SPACE,
        ENTER
    };

    static Key::Type getKeyTypeFromString(const QString& keyTypeString);

    OverlayID getID() const { return _keyID; }
    void setID(OverlayID overlayID) { _keyID = overlayID; }

    void startTimer(int time);
    bool timerFinished();

    void setKeyString(QString keyString) { _keyString = keyString; }
    QString getKeyString(bool toUpper) const;
    int getScanCode(bool toUpper) const;

    bool isPressed() const { return _pressed; }
    void setIsPressed(bool pressed) { _pressed = pressed; }

    void setSwitchToLayerIndex(int layerIndex) { _switchToLayer = layerIndex; }
    int getSwitchToLayerIndex() const { return _switchToLayer; }

    Type getKeyType() const { return _type; }
    void setKeyType(Type type) { _type = type; }

    glm::vec3 getCurrentLocalPosition() const { return _currentLocalPosition; }

    void saveDimensionsAndLocalPosition();

    void scaleKey(float sensorToWorldScale);
private:
    Type _type { Type::CHARACTER };

    int _switchToLayer { 0 };
    bool _pressed { false };

    OverlayID _keyID;
    QString _keyString;

    glm::vec3 _originalLocalPosition;
    glm::vec3 _originalDimensions;
    glm::vec3 _currentLocalPosition;

    std::shared_ptr<QTimer> _timer { std::make_shared<QTimer>() };
};

class Keyboard : public QObject, public Dependency, public ReadWriteLockable {
    Q_OBJECT
public:
    Keyboard();
    void createKeyboard();
    void registerKeyboardHighlighting();
    bool isRaised() const;
    void setRaised(bool raised);
    void setResetKeyboardPositionOnRaise(bool reset);
    bool isPassword() const;
    void setPassword(bool password);
    void enableRightMallet();
    void enableLeftMallet();
    void disableRightMallet();
    void disableLeftMallet();

    void setLeftHandLaser(unsigned int leftHandLaser);
    void setRightHandLaser(unsigned int rightHandLaser);

    void setPreferMalletsOverLasers(bool preferMalletsOverLasers);
    bool getPreferMalletsOverLasers() const;

    bool getUse3DKeyboard() const;
    void setUse3DKeyboard(bool use);
    bool containsID(OverlayID overlayID) const;

    void loadKeyboardFile(const QString& keyboardFile);
    QVector<OverlayID> getKeysID();
    OverlayID getAnchorID();

public slots:
    void handleTriggerBegin(const OverlayID& overlayID, const PointerEvent& event);
    void handleTriggerEnd(const OverlayID& overlayID, const PointerEvent& event);
    void handleTriggerContinue(const OverlayID& overlayID, const PointerEvent& event);
    void handleHoverBegin(const OverlayID& overlayID, const PointerEvent& event);
    void handleHoverEnd(const OverlayID& overlayID, const PointerEvent& event);
    void scaleKeyboard(float sensorToWorldScale);

private:
    struct Anchor {
        OverlayID overlayID;
        glm::vec3 originalDimensions;
    };

    struct BackPlate {
        OverlayID overlayID;
        glm::vec3 dimensions;
        glm::vec3 localPosition;
    };

    struct TextDisplay {
        float lineHeight;
        OverlayID overlayID;
        glm::vec3 localPosition;
        glm::vec3 dimensions;
    };

    void raiseKeyboard(bool raise) const;
    void raiseKeyboardAnchor(bool raise) const;
    void enableStylus();
    void disableStylus();

    void setLayerIndex(int layerIndex);
    void clearKeyboardKeys();
    void switchToLayer(int layerIndex);
    void updateTextDisplay();
    bool shouldProcessOverlayAndPointerEvent(const PointerEvent& event, const OverlayID& overlayID) const;
    bool shouldProcessPointerEvent(const PointerEvent& event) const;
    bool shouldProcessOverlay(const OverlayID& overlayID) const;

    void startLayerSwitchTimer();
    bool isLayerSwitchTimerFinished() const;

    bool _raised { false };
    bool _resetKeyboardPositionOnRaise { true };
    bool _password { false };
    bool _capsEnabled { false };
    int _layerIndex { 0 };
    Setting::Handle<bool> _preferMalletsOverLasers { "preferMalletsOverLaser", true };
    unsigned int _leftHandStylus { 0 };
    unsigned int _rightHandStylus { 0 };
    unsigned int _leftHandLaser { 0 };
    unsigned int _rightHandLaser { 0 };
    SharedSoundPointer _keySound { nullptr };
    std::shared_ptr<QTimer> _layerSwitchTimer { std::make_shared<QTimer>() };

    mutable ReadWriteLockable _use3DKeyboardLock;
    mutable ReadWriteLockable _handLaserLock;
    mutable ReadWriteLockable _preferMalletsOverLasersSettingLock;
    mutable ReadWriteLockable _ignoreItemsLock;
    Setting::Handle<bool> _use3DKeyboard { "use3DKeyboard", true };

    QString _typedCharacters;
    TextDisplay _textDisplay;
    Anchor _anchor;
    BackPlate _backPlate;

    QVector<OverlayID> _itemsToIgnore;
    std::vector<QHash<OverlayID, Key>> _keyboardLayers;
};

#endif
