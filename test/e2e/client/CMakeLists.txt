cmake_minimum_required(VERSION 3.0)
set(DEBUG true CACHE BOOL "do debug build")
set(TEST_OS osx CACHE STRING "OS to test")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c14")
include_directories(. .. ./specs ../../../src/chromium)
include_directories(SYSTEM ../../../src ../../../ext)
if (DEBUG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -DDEBUG")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")
endif()	

file(GLOB_RECURSE src [
	"./main.cpp"
	"./specs/*.cpp"
	"../common.cpp"
])

file(GLOB_RECURSE bench_src [
	"./bench.cpp" 
])

file(GLOB_RECURSE roomcl_src [
	"./roomcl.cpp" 
])

file(GLOB_RECURSE echo_src [
	"./echo.cpp" 
])

if (${TEST_OS} STREQUAL "osx")
	find_library(core_foundation CoreFoundation)
	find_library(cocoa Cocoa)
	find_library(iokit IOKit)
	find_library(security Security)
	find_library(system_configuration SystemConfiguration)
	set(platform_libs ${core_foundation} ${cocoa} ${iokit} ${security} ${system_configuration})
elseif(${TEST_OS} STREQUAL "linux")
	set(platform_libs pthread)
else()
	set(platform_libs "")
endif()
set(platform_lib_paths "../../../build/t/${TEST_OS}")

link_directories(${platform_lib_paths})

add_executable(client ${src})
target_link_libraries(client nq ${platform_libs})

add_executable(bench ${bench_src})
target_link_libraries(bench nq ${platform_libs})

add_executable(roomcl ${roomcl_src})
target_link_libraries(roomcl nq ${platform_libs})

add_executable(echo ${echo_src})
target_link_libraries(echo nq ${platform_libs})
