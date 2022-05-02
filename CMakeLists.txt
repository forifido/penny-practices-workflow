cmake_minimum_required(VERSION 3.6)
project(bidu_hotnews_baobian_server)

set(CMAKE_CXX_STANDARD 11)

set(WORKFLOW_LIB /opt/sogou/lib/libworkflow.a pthread ssl crypto)
#include_directories("/opt/sogou/thirdparth/")
include_directories("/opt/sogou/include")
#add_executable(regrex_demo regrex_demo.cc)
#add_executable(json_demo json_demo.cc)
add_executable(server server.cc)
target_link_libraries(server ${WORKFLOW_LIB})
