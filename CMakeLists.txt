cmake_minimum_required(VERSION 3.0.0)
project(sptManager VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(sptManager main.cpp)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)