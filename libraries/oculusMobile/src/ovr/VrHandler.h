//
//  Created by Bradley Austin Davis on 2018/11/15
//  Copyright 2013-2018 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#pragma once

#include <glm/glm.hpp>
#include <jni.h>
#include <VrApi_Types.h>

#include "TaskQueue.h"

typedef struct ovrMobile ovrMobile;
namespace ovr {

class OculusMobileActivity;

class VrHandler {
    friend class OculusMobileActivity;
public:
    using HandlerTask = std::function<void(VrHandler*)>;
    using OvrMobileTask = std::function<void(ovrMobile*)>;
    using OvrJavaTask = std::function<void(const ovrJava*)>;
    static void setHandler(VrHandler* handler);

    static bool withOvrMobile(const OvrMobileTask& task);
protected:
    static void initVr(const char* appId = nullptr);
    static void shutdownVr();
    static bool withOvrJava(const OvrJavaTask& task);
    void presentFrame(uint32_t textureId, const glm::uvec2& size, const ovrTracking2& tracking) const;
    ovrTracking2 beginFrame();
    bool vrActive() const;
    void pollTask();

private:
    static void submitRenderThreadTask(const HandlerTask& task);
    void updateVrMode(const OculusMobileActivity* activity);
};

}





