#[[
Copyright 2021, Yang Luo"
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

@Author
Yang Luo, PHD
Shenyang Institute of Automation, Chinese Academy of Sciences.
email: luoyang@sia.cn

@Created on: 2023.03.29
]]

cmake_minimum_required(VERSION 3.10)
project(rocos_soem VERSION 0.0.1 DESCRIPTION "ROCOS Simple Open EtherCAT Master")

set(CMAKE_CXX_STANDARD 11)

add_compile_options(-Wno-error=stringop-truncation)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# rocos-ecm install path
include(GNUInstallDirs)
set(CMAKE_INSTALL_PREFIX /opt/rocos/soem)
message(STATUS "INSTALL PREFIX: ${CMAKE_INSTALL_PREFIX}")

find_package(Boost REQUIRED COMPONENTS date_time filesystem system chrono)
find_package(Threads REQUIRED)

add_subdirectory(3rdparty/SOEM)
add_subdirectory(3rdparty/gflags)
add_subdirectory(3rdparty/termcolor)



include_directories(
        include
        include/${PROJECT_NAME}
        src
)


# ecat_config library
add_library(ecat_config SHARED src/ecat_config.cpp)
add_library(${PROJECT_NAME}::ecat_config ALIAS ecat_config)
target_include_directories(ecat_config
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(ecat_config
        PUBLIC
        Boost::boost
        Boost::date_time
        Boost::filesystem
        Boost::system
        rt
)


add_executable(rocos_soem
        src/main.cpp
        src/ecat_config_master.cpp
        src/ecat_flags.cpp
        src/ecat_process.cpp
)
target_link_libraries(rocos_soem
        PUBLIC
        soem
        ecat_config
        gflags::gflags
        pthread
        termcolor::termcolor
)


configure_file(include/ver.h.in ver.h) # Version Definition


enable_testing()
add_executable(unit_test test/unit_test.cpp)
target_link_libraries(unit_test
        PRIVATE
        ecat_config
)
add_test(NAME unit_test COMMAND unit_test)