cmake_minimum_required(VERSION 3.24.1)
project(sptManager VERSION 2.0.0)

include(CTest)
enable_testing()

#
# cross-platform configs
#

# configure c++ version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg -- could alse change the config on vscode
include("C:/Program Files/vcpkg/scripts/buildsystems/vcpkg.cmake") ## change for your system if needed

# get json package from vcpkg
find_package(nlohmann_json CONFIG REQUIRED)

#
# platform dependent configs
#

if(UNIX)
    #
    # linux only config
    #

    # link to linux includes
    include_directories(${PROJECT_SOURCE_DIR}/include)

    # get all linux source files
    file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/src/linux/*.cpp)

    # get pkg-config
    find_package(PkgConfig REQUIRED)

    # link to glib
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    include_directories(${GLIB_INCLUDE_DIRS})
    link_directories(${GLIB_LIBRARY_DIRS})

    # link to x11
    pkg_check_modules(X11 REQUIRED x11)
    include_directories(${X11_INLCUDE_DIRS})
    link_directories(${X11_LIBRARY_DIRS})

    # linux executable configs
    add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
    target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        PRIVATE
        ${GLIB_LIBRARIES}
        ${X11_LIBRARIES}
        ${PROJECT_SOURCE_DIR}/lib/libplayerctl.so.2.4.1
        nlohmann_json::nlohmann_json
        /usr/lib/libgobject-2.0.so.0 # needed for some GLIB symbols
    )
else()
    #
    # windows only config
    #

    # get all windows source files
    file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/src/windows/*.cpp)
    # linux executable configs
    add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
    target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        PRIVATE
        nlohmann_json::nlohmann_json
    )
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)