cmake_minimum_required (VERSION 3.11)

project(SimpleRpc VERSION 0.1.0.0 LANGUAGES CXX)

################################################################################
## Dependencies ################################################################

find_package(Homa REQUIRED)

################################################################################
## Target Definition ###########################################################

add_library(SimpleRpc
    src/Debug.cc
    src/Perf.cc
    src/SimpleRpc.cc
    src/RpcImpl.cc
    src/ServerTaskImpl.cc
    src/SocketImpl.cc
    src/StringUtil.cc
    src/ThreadId.cc
)
add_library(SimpleRpc::SimpleRpc ALIAS SimpleRpc)
target_include_directories(SimpleRpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(SimpleRpc
    PRIVATE
        Homa::Homa
)
target_compile_features(SimpleRpc
    PUBLIC
        cxx_std_11
)
target_compile_options(SimpleRpc
    PRIVATE
        -Wall
        -Wextra
)
set_target_properties(SimpleRpc PROPERTIES
    VERSION ${SimpleRpc_VERSION}
)

################################################################################
## Install & Export ############################################################

install(TARGETS SimpleRpc EXPORT SimpleRpcTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
install(
    DIRECTORY
        include/SimpleRpc
    DESTINATION
        include
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/SimpleRpc/SimpleRpcConfigVersion.cmake"
    VERSION ${SimpleRpc_VERSION}
    COMPATIBILITY SameMinorVersion
)

export(EXPORT SimpleRpcTargets
    NAMESPACE SimpleRpc::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/SimpleRpc/SimpleRpcTargets.cmake"
)

install(EXPORT SimpleRpcTargets
    DESTINATION lib/cmake/SimpleRpc
    NAMESPACE SimpleRpc::
    FILE SimpleRpcTargets.cmake
)

configure_file(cmake/SimpleRpcConfig.cmake
    "${CMAKE_CURRENT_BINARY_DIR}/SimpleRpc/SimpleRpcConfig.cmake"
    COPYONLY
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/SimpleRpc/SimpleRpcConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/SimpleRpc/SimpleRpcConfigVersion.cmake"
        DESTINATION lib/cmake/SimpleRpc
)

################################################################################
##  Unit Tests  ################################################################

include(FetchContent)

## Google Test #################################################################
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.10.0
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(
        ${googletest_SOURCE_DIR}
        ${googletest_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
endif()
include(GoogleTest)

enable_testing()
add_executable(unit_test
    src/DebugTest.cc
    src/SimpleRpcTest.cc
    src/RpcImplTest.cc
    src/ServerTaskImplTest.cc
    src/SocketImplTest.cc
    src/SpinLockTest.cc
    src/StringUtilTest.cc
    src/ThreadIdTest.cc
)
target_link_libraries(unit_test SimpleRpc gmock_main)
# -fno-access-control allows access to private members for testing
target_compile_options(unit_test PRIVATE -fno-access-control)
gtest_discover_tests(unit_test)

################################################################################
##  Additional Tests  ##########################################################

add_subdirectory(test)
