#include "OculusMobileActivity.h"

#include <android/native_window_jni.h>
#include <android/log.h>

using namespace ovr;

extern "C" {

JNIEXPORT jlong JNICALL Java_io_highfidelity_oculus_OculusMobileActivity_nativeOnCreate(JNIEnv* env, jobject obj) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ_JNI", __FUNCTION__);
    auto activity = new OculusMobileActivity();
    activity->nativeOnCreate(env, obj);
    return reinterpret_cast<jlong>(activity);
}

JNIEXPORT void JNICALL Java_io_highfidelity_oculus_OculusMobileActivity_nativeOnDestroy(JNIEnv* env, jobject obj, jlong handle) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ_JNI", __FUNCTION__);
    auto activity = reinterpret_cast<OculusMobileActivity*>(handle);
    activity->nativeOnDestroy(env);
    delete activity;
}

JNIEXPORT void JNICALL Java_io_highfidelity_oculus_OculusMobileActivity_nativeOnResume(JNIEnv* env, jobject obj, jlong handle) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ_JNI", __FUNCTION__);
    reinterpret_cast<OculusMobileActivity*>(handle)->nativeOnResume(env);
}

JNIEXPORT void JNICALL Java_io_highfidelity_oculus_OculusMobileActivity_nativeOnPause(JNIEnv* env, jobject obj, jlong handle) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ_JNI", __FUNCTION__);
    reinterpret_cast<OculusMobileActivity*>(handle)->nativeOnPause(env);
}

JNIEXPORT void JNICALL Java_io_highfidelity_oculus_OculusMobileActivity_nativeOnSurfaceCreated(JNIEnv* env, jobject obj, jlong handle, jobject surface) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ_JNI", __FUNCTION__);
    reinterpret_cast<OculusMobileActivity*>(handle)->nativeSurfaceCreated(ANativeWindow_fromSurface( env, surface ));
}

JNIEXPORT void JNICALL Java_io_highfidelity_oculus_OculusMobileActivity_nativeOnSurfaceChanged(JNIEnv* env, jobject obj, jlong handle, jobject surface) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ_JNI", __FUNCTION__);
    reinterpret_cast<OculusMobileActivity*>(handle)->nativeSurfaceChanged(ANativeWindow_fromSurface( env, surface ));
}

JNIEXPORT void JNICALL Java_io_highfidelity_oculus_OculusMobileActivity_nativeOnSurfaceDestroyed(JNIEnv* env, jobject obj, jlong handle) {
    __android_log_write(ANDROID_LOG_WARN, "QQQ_JNI", __FUNCTION__);
    reinterpret_cast<OculusMobileActivity*>(handle)->nativeSurfaceDestroyed();
}

} // extern "C"
