cmake_minimum_required(VERSION 3.19)
project(Gravity CXX)

set(BUILD_DIR_NAME ${CMAKE_BUILD_TYPE})
if(${CMAKE_CXX_COMPILER} MATCHES "emcc$")
	set(TARGET_WEB TRUE)
	set(BUILD_DIR_NAME ${BUILD_DIR_NAME}Web)
endif()

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

add_executable(Gravity ${SOURCE_FILES} ${IMGUI_FILES})

target_precompile_headers(Gravity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp)

#Adds compiler specific options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	target_compile_options(Gravity PRIVATE -Wall -Wextra -Wshadow -pedantic -msse4.1 --std=c++17 -Wno-unused-parameter -Wno-delete-non-virtual-dtor -Wno-missing-field-initializers)
	set_source_files_properties(Src/Graphics/WaterSimulatorImpl.cpp PROPERTIES COMPILE_FLAGS "-O1 -g0")
	set_source_files_properties(Src/World/Collision.cpp PROPERTIES COMPILE_FLAGS "-O3 -g0")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(Gravity PRIVATE /wd4250 /wd4307 /wd4251 /wd4244 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS
		/D_DISABLE_EXTENDED_ALIGNED_STORAGE)
endif()

target_link_libraries(Gravity stdc++fs tbb yaml-cpp)
target_link_libraries(Gravity EGame yaml-cpp)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(Gravity PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		INSTALL_RPATH "$ORIGIN/rt"
		BUILD_WITH_INSTALL_RPATH TRUE)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_link_options(Gravity PRIVATE "-Wl,-subsystem,windows")
endif()

set_target_properties(Gravity PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}
	OUTPUT_NAME "gravity"
)

string(TIMESTAMP BUILD_DATE "%d-%m-%Y")
target_compile_options(Gravity PRIVATE -DIMGUI_USER_CONFIG="../../Inc/ImGuiConfig.hpp" -DBUILD_DATE="${BUILD_DATE}")

if(NOT ${BUILD_ID} STREQUAL "")
	target_compile_options(Gravity PRIVATE -DBUILD_ID="${BUILD_ID}")
endif()

target_include_directories(Gravity SYSTEM PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Inc
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/imgui
)

function (fixIncludes TARGET)
	get_target_property(INC ${TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	list(REMOVE_ITEM INC "/usr/x86_64-w64-mingw32/include")
	list(REMOVE_ITEM INC "/usr/include")
	set_target_properties(${TARGET} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${INC}")
endfunction()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	set(EMCC_FLAGS "-s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s USE_ZLIB=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s FORCE_FILESYSTEM=1 -s FETCH=1")
	set_target_properties(Gravity PROPERTIES LINK_FLAGS "-s EXTRA_EXPORTED_RUNTIME_METHODS=['cwrap'] -s EXPORTED_FUNCTIONS=['_WebMain'] ${EMCC_FLAGS}")
	
	target_link_libraries(Gravity ${PROTOBUF_DIR}/libprotobuf.a)
	target_include_directories(Gravity SYSTEM PRIVATE ${PROTOBUF_DIR}/include)
else()
	find_package(Protobuf REQUIRED)
	fixIncludes(protobuf::libprotobuf)
	
	target_link_libraries(Gravity protobuf::libprotobuf)
endif()
