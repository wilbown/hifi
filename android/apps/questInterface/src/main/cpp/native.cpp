#include <functional>

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QThread>
#include <QtCore/QStringList>
#include <QtCore/QStandardPaths>
#include <QtCore/QTextStream>
#include <QtCore/QObject>

#include <QtAndroidExtras/QAndroidJniObject>

#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <shared/Storage.h>

#include <AddressManager.h>
#include <AndroidHelper.h>
#include <udt/PacketHeaders.h>

AAssetManager* g_assetManager = nullptr;


extern "C" {

JNIEXPORT jstring JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_getAssetTargetPath(JNIEnv* env, jobject obj) {
    return env->NewStringUTF(QStandardPaths::writableLocation(QStandardPaths::CacheLocation).toUtf8().data());
}


JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeOnCreate(JNIEnv* env, jobject obj, jobject asset_mgr) {
    g_assetManager = AAssetManager_fromJava(env, asset_mgr);
    qRegisterMetaType<QAndroidJniObject>("QAndroidJniObject");
//  interfaceActivity = QAndroidJniObject(obj);
//    QObject::connect(&AndroidHelper::instance(), &AndroidHelper::qtAppLoadComplete, []() {
//        AndroidHelper::instance().moveToThread(qApp->thread());
//        interfaceActivity.callMethod<void>("onAppLoadedComplete", "()V");
//        QObject::disconnect(&AndroidHelper::instance(), &AndroidHelper::qtAppLoadComplete, nullptr, nullptr);
//    });
}

JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeOnDestroy(JNIEnv* env, jobject obj) {
}

JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_SplashActivity_registerLoadCompleteListener(JNIEnv *env,
                                                                               jobject instance) {

}

JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeOnPause(JNIEnv *env, jobject obj) {
    AndroidHelper::instance().notifyEnterBackground();
}

JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_InterfaceActivity_nativeOnResume(JNIEnv *env, jobject obj) {
    AndroidHelper::instance().notifyEnterForeground();
}

JNIEXPORT void JNICALL
Java_io_highfidelity_hifiinterface_receiver_HeadsetStateReceiver_notifyHeadsetOn(JNIEnv *env,
                                                                                 jobject instance,
                                                                                 jboolean pluggedIn) {
    AndroidHelper::instance().notifyHeadsetOn(pluggedIn);
}

}
