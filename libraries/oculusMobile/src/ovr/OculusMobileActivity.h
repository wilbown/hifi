//
//  Created by Bradley Austin Davis on 2018/11/15
//  Copyright 2013-2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once

#include <array>
#include <mutex>

#include <pthread.h>
#include <jni.h>

#include <glm/glm.hpp>

#include <VrApi_Types.h>

#include "VrHandler.h"

typedef struct ANativeWindow ANativeWindow;

namespace ovr {

class OculusMobileActivity {
public:
    void nativeOnCreate(JNIEnv *env, jobject obj);
    void nativeOnResume(JNIEnv *env);
    void nativeOnPause(JNIEnv *env);
    void nativeOnDestroy(JNIEnv *env);
    void nativeSurfaceCreated(ANativeWindow *newNativeWindow);
    void nativeSurfaceChanged(ANativeWindow *newNativeWindow);
    void nativeSurfaceDestroyed();

private:
    void updateVrMode();
    void setNativeWindow(ANativeWindow *);
    void releaseNativeWindow();

public:
    // JNI support
    jobject _activity;
    bool _activityPaused{true};
    ANativeWindow *_nativeWindow{nullptr};
};

}





