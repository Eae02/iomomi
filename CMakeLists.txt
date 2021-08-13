cmake_minimum_required(VERSION 3.16)
project(iomomi CXX)

set(BUILD_DIR_NAME ${CMAKE_BUILD_TYPE})

set(BUILD_ID "" CACHE STRING "Build id to be included in the main menu.")

include("${EGAME_CMAKE_DIR}/EGameTargets.cmake")

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.hpp Src/*.inl Protobuf/Build/*.cc)

set(IMGUI_FILES
	Deps/imgui/imgui.cpp
	Deps/imgui/imgui_demo.cpp
	Deps/imgui/imgui_draw.cpp
	Deps/imgui/imgui_widgets.cpp
	Deps/imgui/imgui_tables.cpp
	Deps/imgui/misc/cpp/imgui_stdlib.cpp
)

add_executable(iomomi ${SOURCE_FILES} ${IMGUI_FILES})

target_precompile_headers(iomomi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp)

#Adds compiler specific options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	target_compile_options(iomomi PRIVATE -Wall -Wextra -Wshadow -pedantic -msse4.1 --std=c++20 -Wno-unused-parameter -Wno-delete-non-virtual-dtor -Wno-missing-field-initializers)
	set_source_files_properties(Src/Graphics/Water/WaterSimulatorImpl.cpp PROPERTIES COMPILE_FLAGS "-O1 -g0")
	set_source_files_properties(Src/World/Collision.cpp PROPERTIES COMPILE_FLAGS "-O3 -g0")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(iomomi PRIVATE /wd4250 /wd4307 /wd4251 /wd4244 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS
		/D_DISABLE_EXTENDED_ALIGNED_STORAGE)
endif()

target_link_libraries(iomomi stdc++fs tbb yaml-cpp)
target_link_libraries(iomomi EGame yaml-cpp)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(iomomi PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		INSTALL_RPATH "$ORIGIN/rt"
		BUILD_WITH_INSTALL_RPATH TRUE)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_link_options(iomomi PRIVATE "-Wl,-subsystem,windows")
endif()

set_target_properties(iomomi PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}
	OUTPUT_NAME "iomomi"
)

string(TIMESTAMP BUILD_DATE "%d-%m-%Y")
target_compile_options(iomomi PRIVATE -DIMGUI_USER_CONFIG="../../Inc/ImGuiConfig.hpp" -DBUILD_DATE="${BUILD_DATE}")

if(NOT ${BUILD_ID} STREQUAL "")
	target_compile_options(iomomi PRIVATE -DBUILD_ID="${BUILD_ID}")
endif()

target_include_directories(iomomi SYSTEM PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Inc
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/imgui
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/pcg/include
)

function (fixIncludes TARGET)
	get_target_property(INC ${TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	list(REMOVE_ITEM INC "/usr/x86_64-w64-mingw32/include")
	list(REMOVE_ITEM INC "/usr/include")
	set_target_properties(${TARGET} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${INC}")
endfunction()

find_package(Protobuf REQUIRED)
fixIncludes(protobuf::libprotobuf)

target_link_libraries(iomomi protobuf::libprotobuf)
