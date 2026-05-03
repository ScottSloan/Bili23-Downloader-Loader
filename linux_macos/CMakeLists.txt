cmake_minimum_required(VERSION 3.10)

project(PyStandLoader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Python3_ROOT_DIR "" CACHE PATH "Python 3.12 prefix directory")
set(OPENSSL_ROOT_DIR "" CACHE PATH "OpenSSL prefix directory")
set(OPENSSL_USE_STATIC_LIBS ON CACHE BOOL "Link OpenSSL statically")

if(Python3_ROOT_DIR)
    message(STATUS "Using Python3 root dir: ${Python3_ROOT_DIR}")
endif()

if(OPENSSL_ROOT_DIR)
    message(STATUS "Using OpenSSL root dir: ${OPENSSL_ROOT_DIR}")
endif()

find_package(Python3 3.12 EXACT REQUIRED COMPONENTS Development)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(loader loader.cpp)

target_link_libraries(loader PRIVATE Python3::Python OpenSSL::SSL OpenSSL::Crypto Threads::Threads dl util m)

if(UNIX AND NOT APPLE)
    target_link_options(loader PRIVATE "-Wl,--export-dynamic")
elseif(APPLE)
    target_link_options(loader PRIVATE "-Wl,-export_dynamic")
endif()

if(APPLE)
    target_link_libraries(loader PRIVATE "-framework CoreFoundation")
endif()