# Created by Hxk
# 2022/09/27

cmake_minimum_required(VERSION 3.16)
project(DocaDma)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (CMAKE_BUILD_TYPE MATCHES Release)
	message(STATUS "Setting build tpye to release")
	SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
else()
	SET(CMAKE_BUILD_TYPE "Debug")
	SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
endif()

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "aarch64")
	set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig")
else()
	set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig")
endif ()

message(STATUS "PKG_CONFIG_PATH is $ENV{PKG_CONFIG_PATH}")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(DOCA_DMA REQUIRED doca-dma)
pkg_check_modules(DOCA_COMMON REQUIRED doca-common)
pkg_check_modules(JSON-C REQUIRED json-c)

message(STATUS "DOCA_DMA_LIBRARY_DIRS ${DOCA_DMA_LIBRARY_DIRS}")
message(STATUS "DOCA_DMA_INCLUDE_DIRS ${DOCA_DMA_INCLUDE_DIRS}")
message(STATUS "DOCA_DMA_LIBRARIES ${DOCA_DMA_LIBRARIES}")

message(STATUS "DOCA_COMMON_LIBRARY_DIRS ${DOCA_COMMON_LIBRARY_DIRS}")
message(STATUS "DOCA_COMMON_INCLUDE_DIRS ${DOCA_COMMON_INCLUDE_DIRS}")
message(STATUS "DOCA_COMMON_LIBRARIES ${DOCA_COMMON_LIBRARIES}")

message(STATUS "JSON-C_LIBRARY_DIRS ${JSON-C_LIBRARY_DIRS}")
message(STATUS "JSON-C_INCLUDE_DIRS ${JSON-C_INCLUDE_DIRS}")
message(STATUS "JSON-C_LIBRARIES ${JSON-C_LIBRARIES}")

message(STATUS ${DOCA_COMMON_LIBRARY_DIRS})
message(STATUS ${DOCA_COMMON_INCLUDE_DIRS})

set(dma_common_file dma_common.c dma_communication.c dma_parameters.c get_clock.c)
set(exec_prog 
    local_dma_read_lat local_dma_read_bw local_dma_write_lat local_dma_write_bw
    remote_dma_read_lat remote_dma_read_bw remote_dma_write_lat remote_dma_write_bw)
# set(exec_prog 
#     remote_dma_read_lat remote_dma_write_lat local_dma_read_lat local_dma_write_lat)

message(STATUS "dma_common_file is ${dma_common_file}")
message(STATUS "exec_prog is ${exec_prog}")
message(STATUS "pthread is ${CMAKE_THREAD_LIBS_INIT}")


foreach(prog ${exec_prog})
	message(STATUS "prog is ${prog}")
	add_executable(${prog} ${prog}.c ${dma_common_file})
	target_include_directories(${prog} PUBLIC
							${GFLAGS_INCLUDE_DIRS}
							${DOCA_COMMON_INCLUDE_DIRS}
							${JSON-C_INCLUDE_DIRS}
							./)
	target_link_directories(${prog} PUBLIC
							${DOCA_COMMON_LIBRARY_DIRS})

	target_link_libraries(${prog}
						  ${GFLAGS_LIBRARIES}
						  ${DOCA_DMA_LIBRARIES}
						  ${DOCA_COMMON_LIBRARIES}
						  ${JSON-C_LIBRARIES}
						  ${CMAKE_THREAD_LIBS_INIT})
endforeach(prog ${exec_prog})
