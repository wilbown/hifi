#include "GLHelpers.h"

#include <mutex>

#include "Config.h"

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtCore/QRegularExpression>
#include <QtCore/QProcessEnvironment>

#include <QtGui/QSurfaceFormat>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLDebugLogger>

#include "Context.h"

size_t evalGLFormatSwapchainPixelSize(const QSurfaceFormat& format) {
    size_t pixelSize = format.redBufferSize() + format.greenBufferSize() + format.blueBufferSize() + format.alphaBufferSize();
    // We don't apply the length of the swap chain into this pixelSize since it is not vsible for the Process (on windows).
    // Let s keep this here remember that:
    // if (format.swapBehavior() > 0) {
    //     pixelSize *= format.swapBehavior(); // multiply the color buffer pixel size by the actual swapchain depth
    // }
    pixelSize += format.stencilBufferSize() + format.depthBufferSize();
    return pixelSize;
}

bool gl::disableGl45() {
#if defined(USE_GLES)
    return false;
#else
    static const QString DEBUG_FLAG("HIFI_DISABLE_OPENGL_45");
    static bool disableOpenGL45 = QProcessEnvironment::systemEnvironment().contains(DEBUG_FLAG);
    return disableOpenGL45;
#endif
}

#ifdef Q_OS_MAC
#define SERIALIZE_GL_RENDERING
#endif

#ifdef SERIALIZE_GL_RENDERING

// This terrible terrible hack brought to you by the complete lack of reasonable
// OpenGL debugging tools on OSX.  Without this serialization code, the UI textures
// frequently become 'glitchy' and get composited onto the main scene in what looks
// like a partially rendered state.
// This looks very much like either state bleeding across the contexts, or bad
// synchronization for the shared OpenGL textures.  However, previous attempts to resolve
// it, even with gratuitous use of glFinish hasn't improved the situation

static std::mutex _globalOpenGLLock;

void gl::globalLock() {
    _globalOpenGLLock.lock();
}

void gl::globalRelease(bool finish) {
    if (finish) {
        glFinish();
    }
    _globalOpenGLLock.unlock();
}

#else

void gl::globalLock() {}
void gl::globalRelease(bool finish) {}

#endif


void gl::getTargetVersion(int& major, int& minor) {
#if defined(USE_GLES)
    major = 3;
    minor = 2;
#else
#if defined(Q_OS_MAC)
    major = 4;
    minor = 1;
#else
    major = 4;
    minor = disableGl45() ? 1 : 5;
#endif
#endif
}

const QSurfaceFormat& getDefaultOpenGLSurfaceFormat() {
    static QSurfaceFormat format;
    static std::once_flag once;
    std::call_once(once, [] {
#if defined(USE_GLES)
        format.setRenderableType(QSurfaceFormat::OpenGLES);
        format.setRedBufferSize(8);
        format.setGreenBufferSize(8);
        format.setBlueBufferSize(8);
        format.setAlphaBufferSize(8);
#else
        format.setProfile(QSurfaceFormat::OpenGLContextProfile::CoreProfile);
#endif
        if (gl::Context::enableDebugLogger()) {
            format.setOption(QSurfaceFormat::DebugContext);
        }
        // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
        format.setDepthBufferSize(DEFAULT_GL_DEPTH_BUFFER_BITS);
        format.setStencilBufferSize(DEFAULT_GL_STENCIL_BUFFER_BITS);
        int major, minor;
        ::gl::getTargetVersion(major, minor);
        format.setMajorVersion(major);
        format.setMinorVersion(minor);
    });
    return format;
}

int glVersionToInteger(QString glVersion) {
    QStringList versionParts = glVersion.split(QRegularExpression("[\\.\\s]"));
    int majorNumber = 0, minorNumber = 0;
    if (versionParts.size() >= 2) {
        majorNumber = versionParts[0].toInt();
        minorNumber = versionParts[1].toInt();
    }
    return (majorNumber << 16) | minorNumber;
}

QJsonObject getGLContextData() {
    static QJsonObject result;
    static std::once_flag once;
    std::call_once(once, [] {
        QString glVersion = QString((const char*)glGetString(GL_VERSION));
        QString glslVersion = QString((const char*) glGetString(GL_SHADING_LANGUAGE_VERSION));
        QString glVendor = QString((const char*) glGetString(GL_VENDOR));
        QString glRenderer = QString((const char*)glGetString(GL_RENDERER));

        result = QJsonObject {
            { "version", glVersion },
            { "sl_version", glslVersion },
            { "vendor", glVendor },
            { "renderer", glRenderer },
        };
    });
    return result;
}

QThread* RENDER_THREAD = nullptr;

bool isRenderThread() {
    return QThread::currentThread() == RENDER_THREAD;
}

#if defined(Q_OS_ANDROID)
#define USE_GLES 1
#endif

namespace gl {
    void withSavedContext(const std::function<void()>& f) {
        // Save the original GL context, because creating a QML surface will create a new context
        QOpenGLContext * savedContext = QOpenGLContext::currentContext();
        QSurface * savedSurface = savedContext ? savedContext->surface() : nullptr;
        f();
        if (savedContext) {
            savedContext->makeCurrent(savedSurface);
        }
    }

    bool checkGLError(const char* name) {
        GLenum error = glGetError();
        if (!error) {
            return false;
        } 
        switch (error) {
            case GL_INVALID_ENUM:
                qCWarning(glLogging) << "GLBackend" << name << ": An unacceptable value is specified for an enumerated argument.The offending command is ignored and has no other side effect than to set the error flag.";
                break;
            case GL_INVALID_VALUE:
                qCWarning(glLogging) << "GLBackend" << name << ": A numeric argument is out of range.The offending command is ignored and has no other side effect than to set the error flag";
                break;
            case GL_INVALID_OPERATION:
                qCWarning(glLogging) << "GLBackend" << name << ": The specified operation is not allowed in the current state.The offending command is ignored and has no other side effect than to set the error flag..";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                qCWarning(glLogging) << "GLBackend" << name << ": The framebuffer object is not complete.The offending command is ignored and has no other side effect than to set the error flag.";
                break;
            case GL_OUT_OF_MEMORY:
                qCWarning(glLogging) << "GLBackend" << name << ": There is not enough memory left to execute the command.The state of the GL is undefined, except for the state of the error flags, after this error is recorded.";
                break;
#if !defined(USE_GLES)
            case GL_STACK_UNDERFLOW:
                qCWarning(glLogging) << "GLBackend" << name << ": An attempt has been made to perform an operation that would cause an internal stack to underflow.";
                break;
            case GL_STACK_OVERFLOW:
                qCWarning(glLogging) << "GLBackend" << name << ": An attempt has been made to perform an operation that would cause an internal stack to overflow.";
                break;
#endif
        }
        return true;
    }


    bool checkGLErrorDebug(const char* name) {
        // Disabling error checking macro on Android debug builds for now, 
        // as it throws off performance testing, which must be done on 
        // Debug builds
#if defined(DEBUG) && !defined(Q_OS_ANDROID)
        return checkGLError(name);
#else
        Q_UNUSED(name);
        return false;
#endif
    }

    // Enables annotation of captures made by tools like renderdoc
    bool khrDebugEnabled() {
        static std::once_flag once;
        static bool khrDebug = false;
        std::call_once(once, [&] {
            khrDebug = nullptr != glPushDebugGroupKHR;
        });
        return khrDebug;
    }

    // Enables annotation of captures made by tools like renderdoc
    bool extDebugMarkerEnabled() {
        static std::once_flag once;
        static bool extMarker = false;
        std::call_once(once, [&] {
            extMarker = nullptr != glPushGroupMarkerEXT;
        });
        return extMarker;
    }

    bool debugContextEnabled() {
#if defined(Q_OS_MAC)
        // OSX does not support GL_KHR_debug or GL_ARB_debug_output
        static bool enableDebugLogger = false;
#elif defined(DEBUG) || defined(USE_GLES)
        //static bool enableDebugLogger = true;
        static bool enableDebugLogger = false;
#else
        static const QString DEBUG_FLAG("HIFI_DEBUG_OPENGL");
        static bool enableDebugLogger = QProcessEnvironment::systemEnvironment().contains(DEBUG_FLAG);
#endif
        return enableDebugLogger;
    }
}
