set(TARGET_NAME native-lib)
setup_hifi_library()
link_hifi_libraries(
    shared task networking qml 
    image fbx hfm render-utils physics entities octree 
    oculusMobile oculusMobilePlugin
    gl gpu ${PLATFORM_GL_BACKEND}
)
target_opengl()
target_bullet()

message("QQQ CMAKE_SOURCE_DIR ${CMAKE_SOURCE_DIR}")
add_subdirectory("${CMAKE_SOURCE_DIR}/interface" "libraries/interface")
include_directories("${CMAKE_SOURCE_DIR}/interface/src")
add_subdirectory("${CMAKE_SOURCE_DIR}/plugins/hifiCodec" "libraries/hifiCodecPlugin")

target_link_libraries(native-lib android log m interface)

#set(GVR_ROOT "${HIFI_ANDROID_PRECOMPILED}/gvr/gvr-android-sdk-1.101.0/")
#target_include_directories(native-lib PRIVATE  "${GVR_ROOT}/libraries/headers" "libraries/ui/src")
#target_link_libraries(native-lib "${GVR_ROOT}/libraries/libgvr.so" ui)
