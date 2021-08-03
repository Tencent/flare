# Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
#
# Licensed under the BSD 3-Clause License (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of the
# License at
#
# https://opensource.org/licenses/BSD-3-Clause
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

cmake_minimum_required(VERSION 3.16)

project(flare C CXX)

set(CMAKE_CXX_STANDARD 2a)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/build/cmake)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8)
        message(FATAL_ERROR "GCC 8 or higher is required for this project.")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
        message(FATAL_ERROR "Clang 9 or higher is required for this project.")
    endif()
else()
    message(WARNING "Only GCC or Clang is supported.")
endif()

find_package(CURL REQUIRED)
find_package(FMT REQUIRED)
# Optional. This is used only by /inspect pages. Most of the users does not use
# those pages much anyway.
# find_package(CTEMPLATE)
find_package(GFLAGS REQUIRED)
find_package(GLOG REQUIRED)
# Optional. Only some profiler handlers require this.
find_package(GPERFTOOLS)
# Optional. Only examples and jemalloc profiler handler require this.
find_package(JEMALLOC)
find_package(JSONCPP REQUIRED)
find_package(LZ4 REQUIRED)
find_package(OPENTRACING_CPP REQUIRED)  # Only 1.5.0 is supported?
find_package(PROTOBUF REQUIRED)
# Optional. For the moment only COS client uses this.
find_package(RAPIDXML)
find_package(SNAPPY)
# Optional. Only base/experimental/bloom_filter uses this.
#find_package(XXHASH)
find_package(YAML_CPP)
find_package(ZLIB)
find_package(ZSTD)

#---------------------------------------------------------------------------------------
# 设置ASM的编译选项
#---------------------------------------------------------------------------------------
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}  -fstack-protector -Wall -Wunused-but-set-parameter")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}  -fno-omit-frame-pointer -Wno-free-nonheap-object")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}  -MD -MF -fPIC -O2")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS}  -fno-canonical-system-headers -Wno-builtin-macro-redefined")


#---------------------------------------------------------------------------------------
# 设置CPP编译选项
#---------------------------------------------------------------------------------------
if(TRPC_GCC_VERSION_GREATER)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-error=class-memaccess")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wno-implicit-fallthrough -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_METRICS_M007=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_METRICS_MM=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_TRACING_TJG=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_ATTA_API=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_CONFIG_TCONF=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_NAMING_POLARIS=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_CONFIG_RAINBOW=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_METRICS_ZHIYAN=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_ZHIYAN_LOG=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_JCE_TOOL=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_ASN1=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_ASN2=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_VIDEOPACK=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_SSO=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_OIDB=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_CRMQ=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_ISTORE=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DBUILD_EXCLUDE_NRPC=1")
