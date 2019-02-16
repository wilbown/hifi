//
//  AudioInjector.h
//  libraries/audio/src
//
//  Created by Stephen Birarda on 1/2/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_AudioInjector_h
#define hifi_AudioInjector_h

#include <memory>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>
#include <QtCore/QThread>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <NLPacket.h>

#include "AudioInjectorLocalBuffer.h"
#include "AudioInjectorOptions.h"
#include "AudioHRTF.h"
#include "AudioFOA.h"
#include "Sound.h"

class AbstractAudioInterface;
class AudioInjectorManager;
class AudioInjector;
using AudioInjectorPointer = QSharedPointer<AudioInjector>;


enum class AudioInjectorState : uint8_t {
    NotFinished = 0,
    Finished = 1,
    PendingDelete = 2,
    LocalInjectionFinished = 4,
    NetworkInjectionFinished = 8
};

AudioInjectorState operator& (AudioInjectorState lhs, AudioInjectorState rhs);
AudioInjectorState& operator|= (AudioInjectorState& lhs, AudioInjectorState rhs);

// In order to make scripting cleaner for the AudioInjector, the script now holds on to the AudioInjector object
// until it dies.
class AudioInjector : public QObject, public QEnableSharedFromThis<AudioInjector> {
    Q_OBJECT
public:
    AudioInjector(SharedSoundPointer sound, const AudioInjectorOptions& injectorOptions);
    AudioInjector(AudioDataPointer audioData, const AudioInjectorOptions& injectorOptions);
    ~AudioInjector();

    bool isFinished() const { return (stateHas(AudioInjectorState::Finished)); }

    int getCurrentSendOffset() const { return _currentSendOffset; }
    void setCurrentSendOffset(int currentSendOffset) { _currentSendOffset = currentSendOffset; }

    AudioInjectorLocalBuffer* getLocalBuffer() const { return _localBuffer; }
    AudioHRTF& getLocalHRTF() { return _localHRTF; }
    AudioFOA& getLocalFOA() { return _localFOA; }

    bool isLocalOnly() const { return _options.localOnly; }
    float getVolume() const { return _options.volume; }
    bool isPositionSet() const { return _options.positionSet; }
    glm::vec3 getPosition() const { return _options.position; }
    glm::quat getOrientation() const { return _options.orientation; }
    bool isStereo() const { return _options.stereo; }
    bool isAmbisonic() const { return _options.ambisonic; }

    bool stateHas(AudioInjectorState state) const ;
    static void setLocalAudioInterface(AbstractAudioInterface* audioInterface) { _localAudioInterface = audioInterface; }

    static AudioInjectorPointer playSoundAndDelete(SharedSoundPointer sound, const AudioInjectorOptions& options);
    static AudioInjectorPointer playSound(SharedSoundPointer sound, const AudioInjectorOptions& options);
    static AudioInjectorPointer playSoundAndDelete(AudioDataPointer audioData, const AudioInjectorOptions& options);
    static AudioInjectorPointer playSound(AudioDataPointer audioData, const AudioInjectorOptions& options);

public slots:
    void restart();

    void stop();
    void triggerDeleteAfterFinish();

    const AudioInjectorOptions& getOptions() const { return _options; }
    void setOptions(const AudioInjectorOptions& options);

    float getLoudness() const { return _loudness; }
    bool isPlaying() const { return !stateHas(AudioInjectorState::Finished); }
    void finish();
    void finishLocalInjection();
    void finishNetworkInjection();

signals:
    void finished();
    void restarting();

private:
    int64_t injectNextFrame();
    bool inject(bool(AudioInjectorManager::*injection)(const AudioInjectorPointer&));
    bool injectLocally();
    void deleteLocalBuffer();

    static AbstractAudioInterface* _localAudioInterface;

    const SharedSoundPointer _sound;
    AudioDataPointer _audioData;
    AudioInjectorOptions _options;
    AudioInjectorState _state { AudioInjectorState::NotFinished };
    bool _hasSentFirstFrame { false };
    float _loudness { 0.0f };
    int _currentSendOffset { 0 };
    std::unique_ptr<NLPacket> _currentPacket { nullptr };
    AudioInjectorLocalBuffer* _localBuffer { nullptr };

    int64_t _nextFrame { 0 };
    std::unique_ptr<QElapsedTimer> _frameTimer { nullptr };
    quint16 _outgoingSequenceNumber { 0 };

    // when the injector is local, we need this
    AudioHRTF _localHRTF;
    AudioFOA _localFOA;
    friend class AudioInjectorManager;
};

#endif // hifi_AudioInjector_h
