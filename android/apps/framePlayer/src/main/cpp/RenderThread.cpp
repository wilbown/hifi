//
//  Created by Bradley Austin Davis on 2018/10/21
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "RenderThread.h"

#include <mutex>

#include <jni.h>
#include <android/log.h>

#include <QtCore/QFileInfo>
#include <QtGui/QWindow>
#include <QtGui/QImageReader>

#include <gl/QOpenGLContextWrapper.h>
#include <gpu/FrameIO.h>
#include <gpu/Texture.h>

#if OCULUS_MOBILE
#include <VrApi_Types.h>
#include <ovr/VrHandler.h>
#include <ovr/Helpers.h>

#include <VrApi.h>
#include <VrApi_Input.h>

static JNIEnv* _env { nullptr };
static JavaVM* _vm { nullptr };
static jobject _activity { nullptr };

struct HandController{
    ovrInputTrackedRemoteCapabilities caps;
    ovrInputStateTrackedRemote state;
    ovrResult stateResult{ ovrSuccess };
    ovrTracking tracking;
    ovrResult trackingResult{ ovrSuccess };

    void update(ovrMobile* session, double time = 0.0) {
        const auto& deviceId = caps.Header.DeviceID;
        stateResult = vrapi_GetCurrentInputState(session, deviceId, &state.Header);
        trackingResult = vrapi_GetInputTrackingState(session, deviceId, 0.0, &tracking);
    }
};

std::vector<HandController> devices;

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ", __FUNCTION__);
    return JNI_VERSION_1_6;
}


JNIEXPORT void JNICALL Java_io_highfidelity_frameplayer_FramePlayerActivity_nativeOnCreate(JNIEnv* env, jobject obj) {
    env->GetJavaVM(&_vm);
    _activity = env->NewGlobalRef(obj);
}
}

#else
extern "C" {
JNIEXPORT void JNICALL Java_io_highfidelity_frameplayer_FramePlayerActivity_nativeOnCreate(JNIEnv* env, jobject obj) {
}
}
#endif

static const char* FRAME_FILE = "assets:/frames/20181124_1047.json";

static void textureLoader(const std::string& filename, const gpu::TexturePointer& texture, uint16_t layer) {
    QImage image;
    QImageReader(filename.c_str()).read(&image);
    if (layer > 0) {
        return;
    }
    texture->assignStoredMip(0, image.byteCount(), image.constBits());
}

void RenderThread::submitFrame(const gpu::FramePointer& frame) {
    std::unique_lock<std::mutex> lock(_frameLock);
    _pendingFrames.push(frame);
}

void RenderThread::move(const glm::vec3& v) {
    std::unique_lock<std::mutex> lock(_frameLock);
    _correction = glm::inverse(glm::translate(mat4(), v)) * _correction;
}

void RenderThread::initialize(QWindow* window) {
    if (QFileInfo(FRAME_FILE).exists()) {
        auto frame = gpu::readFrame(FRAME_FILE, _externalTexture, &textureLoader);
        submitFrame(frame);
    }

    std::unique_lock<std::mutex> lock(_frameLock);
    setObjectName("RenderThread");
    Parent::initialize();
    _window = window;
    _glContext.setWindow(window);
    _glContext.create();
    _glContext.makeCurrent();

    glGenTextures(1, &_externalTexture);
    glBindTexture(GL_TEXTURE_2D, _externalTexture);
    static const glm::u8vec4 color{ 0 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &color);
    gl::setSwapInterval(0);

    // GPU library init
    gpu::Context::init<gpu::gl::GLBackend>();
    _glContext.makeCurrent();
    _gpuContext = std::make_shared<gpu::Context>();
    _backend = _gpuContext->getBackend();
    _glContext.doneCurrent();
    _glContext.moveToThread(_thread);
    _thread->setObjectName("RenderThread");
}

void RenderThread::setup() {
    ovr::VrHandler::initVr();

    // Wait until the context has been moved to this thread
    { std::unique_lock<std::mutex> lock(_frameLock); }
    _gpuContext->beginFrame();
    _gpuContext->endFrame();
    _glContext.makeCurrent();


#if OCULUS_MOBILE
    _vm->AttachCurrentThread(&_env, nullptr);
    jclass cls = _env->GetObjectClass(_activity);
    jmethodID mid = _env->GetMethodID(cls, "launchOculusActivity", "()V");
    _env->CallVoidMethod(_activity, mid);
    ovr::VrHandler::setHandler(this);
#endif
}

void RenderThread::shutdown() {
    _activeFrame.reset();
    while (!_pendingFrames.empty()) {
        _gpuContext->consumeFrameUpdates(_pendingFrames.front());
        _pendingFrames.pop();
    }
    _gpuContext->shutdown();
    _gpuContext.reset();
}

void RenderThread::handleInput() {
#if OCULUS_MOBILE
    auto readResult = ovr::VrHandler::withOvrMobile([&](ovrMobile *session) {
        for (auto &controller : devices) {
            controller.update(session);
        }
    });

    if (readResult) {
        for (auto &controller : devices) {
            const auto &caps = controller.caps;
            if (controller.stateResult >= 0) {
                const auto &remote = controller.state;
                if (remote.Joystick.x != 0.0f || remote.Joystick.y != 0.0f) {
                    glm::vec3 translation;
                    float rotation = 0.0f;
                    if (caps.ControllerCapabilities & ovrControllerCaps_LeftHand) {
                        translation = glm::vec3{0.0f, -remote.Joystick.y, 0.0f};
                    } else {
                        translation = glm::vec3{remote.Joystick.x, 0.0f, -remote.Joystick.y};
                    }
                    float scale = 0.1f + (1.9f * remote.GripTrigger);
                    _correction = glm::translate(glm::mat4(), translation * scale) * _correction;
                }
            }
        }
    }
#endif
}

void RenderThread::renderFrame(gpu::FramePointer& frame) {
    auto& eyeProjections = _activeFrame->stereoState._eyeProjections;
    auto& eyeOffsets = _activeFrame->stereoState._eyeViews;
#if OCULUS_MOBILE
    const auto& tracking = beginFrame();
    // Quest
    auto frameCorrection = _correction * ovr::toGlm(tracking.HeadPose.Pose);
    _backend->setCameraCorrection(glm::inverse(frameCorrection), _activeFrame->view);
    ovr::for_each_eye([&](ovrEye eye){
        const auto& eyeInfo = tracking.Eye[eye];
        eyeProjections[eye] = ovr::toGlm(eyeInfo.ProjectionMatrix);
        eyeOffsets[eye] = ovr::toGlm(eyeInfo.ViewMatrix);
    });

    static std::once_flag once;
    std::call_once(once, [&]{
        withOvrMobile([&](ovrMobile* session){
            int deviceIndex = 0;
            ovrInputCapabilityHeader capsHeader;
            while (vrapi_EnumerateInputDevices(session, deviceIndex, &capsHeader) >= 0) {
                if (capsHeader.Type == ovrControllerType_TrackedRemote) {
                    HandController controller = {};
                    controller.caps.Header = capsHeader;
                    controller.state.Header.ControllerType = ovrControllerType_TrackedRemote;
                    vrapi_GetInputDeviceCapabilities( session, &controller.caps.Header);
                    devices.push_back(controller);
                }
                ++deviceIndex;
            }
        });
    });
#else
    eyeProjections[0][0] = vec4{ 0.91729, 0.0, -0.17407, 0.0 };
    eyeProjections[0][1] = vec4{ 0.0, 0.083354, -0.106141, 0.0 };
    eyeProjections[0][2] = vec4{ 0.0, 0.0, -1.0, -0.2 };
    eyeProjections[0][3] = vec4{ 0.0, 0.0, -1.0, 0.0 };
    eyeProjections[1][0] = vec4{ 0.91729, 0.0, 0.17407, 0.0 };
    eyeProjections[1][1] = vec4{ 0.0, 0.083354, -0.106141, 0.0 };
    eyeProjections[1][2] = vec4{ 0.0, 0.0, -1.0, -0.2 };
    eyeProjections[1][3] = vec4{ 0.0, 0.0, -1.0, 0.0 };
    eyeOffsets[0][3] = vec4{ -0.0327499993, 0.0, -0.0149999997, 1.0 };
    eyeOffsets[1][3] = vec4{ 0.0327499993, 0.0, -0.0149999997, 1.0 };
#endif
    _glContext.makeCurrent();
    _backend->recycle();
    _backend->syncCache();
    _gpuContext->enableStereo(true);
    if (frame && !frame->batches.empty()) {
        _gpuContext->executeFrame(frame);
    }
    auto& glbackend = (gpu::gl::GLBackend&)(*_backend);
    glm::uvec2 fboSize{ frame->framebuffer->getWidth(), frame->framebuffer->getHeight() };

#if OCULUS_MOBILE
    auto finalTexture = glbackend.getTextureID(frame->framebuffer->getRenderBuffer(0));
    _glContext.doneCurrent();
    presentFrame(finalTexture, fboSize, tracking);
#else
    auto windowSize = _window->geometry().size();
    auto finalFbo = glbackend.getFramebufferID(frame->framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, finalFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClearColor(1, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(
        0, 0, fboSize.x, fboSize.y,
        0, 0, windowSize.width(), windowSize.height(),
        GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    CHECK_GL_ERROR();
    _glContext.swapBuffers();
#endif
}

bool RenderThread::process() {
#if OCULUS_MOBILE
    pollTask();

    if (!vrActive()) {
        QThread::msleep(1);
        return true;
    }
#endif

    std::queue<gpu::FramePointer> pendingFrames;
    {
        std::unique_lock<std::mutex> lock(_frameLock);
        pendingFrames.swap(_pendingFrames);
    }

    while (!pendingFrames.empty()) {
        _activeFrame = pendingFrames.front();
        pendingFrames.pop();
        _gpuContext->consumeFrameUpdates(_activeFrame);
        _activeFrame->stereoState._enable = true;
    }

    if (!_activeFrame) {
        _glContext.makeCurrent();
        glClearColor(1, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        CHECK_GL_ERROR();
        _glContext.swapBuffers();
        QThread::msleep(1);
        return true;
    }
    handleInput();
    renderFrame(_activeFrame);
    return true;
}
