    //
//  Created by Bradley Austin Davis on 2018/11/15
//  Copyright 2013-2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "OculusMobileActivity.h"

#include <atomic>

#include <android/log.h>
#include <android/native_window_jni.h>

#include <VrApi.h>

using namespace ovr;

void OculusMobileActivity::updateVrMode() {
    VrHandler::submitRenderThreadTask([&](VrHandler* handler){
        bool vrReady = !_activityPaused && _nativeWindow != nullptr;
        handler->updateVrMode(vrReady ? this : nullptr);
    });
}

void OculusMobileActivity::setNativeWindow(ANativeWindow *newNativeWindow) {
    if (newNativeWindow != _nativeWindow) {
        releaseNativeWindow();
    }
    _nativeWindow = newNativeWindow;
    updateVrMode();
}

void OculusMobileActivity::releaseNativeWindow() {
    if (_nativeWindow != nullptr) {
        auto oldNativeWindow = _nativeWindow;
        _nativeWindow = nullptr;
        updateVrMode();
        ANativeWindow_release(oldNativeWindow);
    }
}

void OculusMobileActivity::nativeOnCreate(JNIEnv *env, jobject obj) {
    _activity = env->NewGlobalRef(obj);
}

void OculusMobileActivity::nativeOnDestroy(JNIEnv *env) {
    env->DeleteGlobalRef(_activity);
    _activity = nullptr;
}

void OculusMobileActivity::nativeOnResume(JNIEnv *env) {
    _activityPaused = false;
    updateVrMode();
}

void OculusMobileActivity::nativeOnPause(JNIEnv *env) {
    _activityPaused = true;
    updateVrMode();
}

void OculusMobileActivity::nativeSurfaceCreated(ANativeWindow *newNativeWindow) {
    setNativeWindow(newNativeWindow);
}

void OculusMobileActivity::nativeSurfaceChanged(ANativeWindow *newNativeWindow) {
    setNativeWindow(newNativeWindow);
}

void OculusMobileActivity::nativeSurfaceDestroyed() {
    releaseNativeWindow();
}
