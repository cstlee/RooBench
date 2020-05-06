# Copyright (c) 2020, Stanford University
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

cmake_minimum_required(VERSION 3.11)

project(RooBench VERSION 0.0.1 LANGUAGES CXX)

# Tell CMake where to find our custom/3rd-party "Find" modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

find_package(Homa COMPONENTS DpdkDriver)
find_package(Roo)
find_package(PerfUtils)
find_package(Threads)

# Source control tool; needed to download external libraries.
find_package(Git REQUIRED)

include(FetchContent)
FetchContent_Declare(docopt
    GIT_REPOSITORY https://github.com/docopt/docopt.cpp.git
    GIT_TAG        3dd23e3280f213bacefdf5fcb04857bf52e90917
)

FetchContent_GetProperties(docopt)
if(NOT docopt_POPULATED)
    FetchContent_Populate(docopt)
    add_subdirectory(
        ${docopt_SOURCE_DIR}
        ${docopt_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
endif()

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.7.3
)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_executable(server
    src/main.cc
    src/Benchmark.cc
    src/DpcBenchmark.cc
)
target_link_libraries(server
    PRIVATE
        docopt
        nlohmann_json::nlohmann_json
        Threads::Threads
        Roo::Roo
        Homa::Homa
        Homa::DpdkDriver
        PerfUtils
)

add_executable(getmac
    src/getmac.cc
)
target_link_libraries(getmac
    PRIVATE
        Homa::DpdkDriver
        docopt
        nlohmann_json::nlohmann_json
)