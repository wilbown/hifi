//
//  Created by Bradley Austin Davis on 2018/10/21
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#pragma once

#define OCULUS_MOBILE 1

#include <QtCore/QElapsedTimer>

#include <GenericThread.h>
#include <shared/RateCounter.h>
#include <gl/Config.h>
#include <gl/Context.h>
#include <gpu/gl/GLBackend.h>

#if OCULUS_MOBILE
#include <ovr/OculusMobileActivity.h>
#include <ovr/VrHandler.h>
#endif

#if OCULUS_MOBILE
class RenderThread : public GenericThread, ovr::VrHandler {
#else
class RenderThread : public GenericThread {
#endif
    using Parent = GenericThread;
public:
    QWindow* _window{ nullptr };
    gl::Context _glContext;
    std::mutex _mutex;
    gpu::ContextPointer _gpuContext;  // initialized during window creation
    std::shared_ptr<gpu::Backend> _backend;
    std::atomic<size_t> _presentCount{ 0 };
    std::mutex _frameLock;
    std::queue<gpu::FramePointer> _pendingFrames;
    gpu::FramePointer _activeFrame;
    uint32_t _externalTexture{ 0 };
    glm::mat4 _correction;

#if OCULUS_MOBILE
    JavaVM* getVm() const override;
    JNIEnv* getEnv() const override;
    jobject getActivity() const override;
#endif

    void move(const glm::vec3& v);
    void setup() override;
    bool process() override;
    void shutdown() override;

    void handleInput();

    void submitFrame(const gpu::FramePointer& frame);
    void initialize(QWindow* window);
    void renderFrame(gpu::FramePointer& frame);
};
