set(TARGET_NAME gym)
setup_hifi_library(Network)
link_hifi_libraries(shared networking)

target_link_libraries(${TARGET_NAME} zmq)
