add_executable(client client.cpp)
target_link_libraries(client ${Boost_LIBRARIES} ${SSL_LIBRARIES} ${CASABLANCA_LIBRARY} ${AZURE_STORAGE_LIBRARY})
