set(TARGET_NAME gym)
setup_hifi_library()
link_hifi_libraries(shared)

target_link_libraries(${TARGET_NAME} zmq)
