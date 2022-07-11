FILE(GLOB MASTER_SRC *.cpp)
include_directories(${CMAKE_BINARY_DIR}/proto)
add_executable(master master.cpp)
target_link_libraries(master hw_grpc_proto ${_REFLECTION}  ${ZOOKEEPER_LIBRARIES} ${CONSERVATOR_LIBRARIES} ${_GRPC_GRPCPP} ${_PROTOBUF_LIBPROTOBUF} ${Boost_LIBRARIES}  ${SSL_LIBRARIES} ${CASABLANCA_LIBRARY} ${AZURE_STORAGE_LIBRARY})
