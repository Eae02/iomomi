cmake_minimum_required(VERSION 3.13)
project(Gravity)

set(BUILD_DIR_NAME ${CMAKE_BUILD_TYPE})
if(${CMAKE_CXX_COMPILER} MATCHES "emcc$")
	set(TARGET_WEB TRUE)
	set(BUILD_DIR_NAME ${BUILD_DIR_NAME}Web)
endif()

option(YAML_CPP_BUILD_TESTS OFF)
add_subdirectory(Deps/yaml-cpp)

include("${EGAME_CMAKE_DIR}/EGameTargets.cmake")

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.hpp Protobuf/Build/*.cc)

set(IMGUI_FILES Deps/imgui/imconfig.h Deps/imgui/imgui.h Deps/imgui/imgui.cpp Deps/imgui/imgui_demo.cpp Deps/imgui/imgui_draw.cpp
	Deps/imgui/imgui_internal.h Deps/imgui/imgui_widgets.cpp Deps/imgui/imstb_rectpack.h Deps/imgui/imstb_textedit.h
	Deps/imgui/imstb_truetype.h Deps/imgui/misc/cpp/imgui_stdlib.cpp Deps/imgui/misc/cpp/imgui_stdlib.h)

add_executable(Gravity ${SOURCE_FILES} ${IMGUI_FILES})

#Adds compiler specific options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	target_compile_options(Gravity PRIVATE -Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-missing-field-initializers
		$<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp>)
	if (NOT TARGET_WEB)
		target_link_libraries(Gravity stdc++fs)
		set_target_properties(Gravity PROPERTIES
			CXX_VISIBILITY_PRESET hidden
			INSTALL_RPATH "$ORIGIN/rt"
			BUILD_WITH_INSTALL_RPATH TRUE)
	endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(Gravity PRIVATE /wd4250 /wd4307 /wd4251 /wd4244 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS
		/D_DISABLE_EXTENDED_ALIGNED_STORAGE $<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp>)
endif()

set_target_properties(Gravity PROPERTIES
	CXX_STANDARD 17
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${BUILD_DIR_NAME}
	OUTPUT_NAME "gravity"
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

target_link_libraries(Gravity EGame yaml-cpp)

if (TARGET_WEB)
	set(EMCC_FLAGS "-s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s USE_BULLET=1 -s USE_ZLIB=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s FORCE_FILESYSTEM=1 -s FETCH=1")
	set_target_properties(Gravity PROPERTIES LINK_FLAGS "-s EXTRA_EXPORTED_RUNTIME_METHODS=['cwrap'] -s EXPORTED_FUNCTIONS=['_WebMain'] ${EMCC_FLAGS}")
	target_compile_options(Gravity PRIVATE -s USE_BULLET=1)
	
	target_link_libraries(Gravity ${PROTOBUF_DIR}/libprotobuf.a)
	target_include_directories(Gravity SYSTEM PRIVATE ${PROTOBUF_DIR}/include)
else()
	set(Protobuf_USE_STATIC_LIBS ON)
	
	find_package(Bullet REQUIRED)
	find_package(Protobuf REQUIRED)
	
	target_link_libraries(Gravity protobuf::libprotobuf ${BULLET_LIBRARIES})
	target_include_directories(Gravity SYSTEM PRIVATE ${BULLET_INCLUDE_DIRS})
endif()
