cmake_minimum_required(VERSION 3.13)
project(Gravity)

find_package(Threads REQUIRED)

if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
	set(ARCH_SUFFIX "64")
else()
	set(ARCH_SUFFIX "32")
endif()

option(YAML_CPP_BUILD_TESTS OFF)
add_subdirectory(Deps/yaml-cpp)

include("${EGAME_CMAKE_DIR}/EGameTargets.cmake")

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.hpp)

set(IMGUI_FILES Deps/imgui/imconfig.h Deps/imgui/imgui.h Deps/imgui/imgui.cpp Deps/imgui/imgui_demo.cpp Deps/imgui/imgui_draw.cpp
	Deps/imgui/imgui_internal.h Deps/imgui/imgui_widgets.cpp Deps/imgui/imstb_rectpack.h Deps/imgui/imstb_textedit.h
	Deps/imgui/imstb_truetype.h Deps/imgui/misc/cpp/imgui_stdlib.cpp Deps/imgui/misc/cpp/imgui_stdlib.h)

add_executable(Gravity ${SOURCE_FILES} ${IMGUI_FILES})

#Adds compiler specific options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	target_compile_options(Gravity PRIVATE -Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-missing-field-initializers
		$<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp>)
	target_link_libraries(Gravity stdc++fs)
	set_target_properties(Gravity PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		INSTALL_RPATH "$ORIGIN/rt${ARCH_SUFFIX}"
		BUILD_WITH_INSTALL_RPATH TRUE)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(Gravity PRIVATE /wd4250 /wd4251 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(Gravity PROPERTIES
	CXX_STANDARD 17
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${CMAKE_BUILD_TYPE}
	OUTPUT_NAME "gravity${ARCH_SUFFIX}"
)

target_compile_options(Gravity PRIVATE
	-DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL
	-DIMGUI_USER_CONFIG="../../Inc/ImGuiConfig.hpp")

target_include_directories(Gravity SYSTEM PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Inc
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/utfcpp/source
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/json/include
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/imgui
)

target_link_libraries(Gravity
	EGame
	yaml-cpp
)
