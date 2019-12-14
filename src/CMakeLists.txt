include(FindProtobuf)

if (NOT PROTOBUF_PROTOC_EXECUTABLE)
    get_filename_component(PROTO_LIB_DIR ${PROTOBUF_LIBRARY} DIRECTORY)
    set (PROTOBUF_PROTOC_EXECUTABLE "${PROTO_LIB_DIR}/../bin/protoc")
endif()

protobuf_generate_cpp(PROTO_SRC PROTO_HEADER config.proto rpc.proto storage.proto)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PROTOBUF_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/third_party/libco
        ${PROJECT_SOURCE_DIR}/third_party/pb2json/byReflection
        ${PROJECT_SOURCE_DIR}/src/Core
        ${PROJECT_SOURCE_DIR}/src)

set(SRC Log.cpp  Core/Config.cpp Core/Msg.cpp Core/Network.cpp Core/Storage.cpp Core/Utils.cpp)
add_library(tpc ${SRC})

add_executable(client client.cpp   ${PROTO_SRC} ${PROTO_HEADER})
target_link_libraries(client tpc rocksdb  pb2json   protobuf  colib_shared dl)

add_executable(logicServ logicServ.cpp   ${PROTO_SRC} ${PROTO_HEADER})
target_link_libraries(logicServ tpc  rocksdb  pb2json   protobuf colib_shared dl)

add_executable(storageServ storageServ.cpp   ${PROTO_SRC} ${PROTO_HEADER})
target_link_libraries(storageServ tpc  rocksdb pb2json   protobuf  colib_shared dl)