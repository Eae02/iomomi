cmake_minimum_required(VERSION 3.16)
project(iomomi)

set(BUILD_DIR_NAME ${CMAKE_BUILD_TYPE})

set(BUILD_ID "" CACHE STRING "Build id to be included in the main menu.")

option(IOMOMI_EDITOR "Whether or not to enable the editor." ON)
option(IOMOMI_WATER "Whether or not to enable water." ON)

set(EG_BUILD_ASSETMAN OFF CACHE BOOL "" FORCE)
set(EG_BUILD_IMGUI ${IOMOMI_EDITOR} CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Deps/egame_override")
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/egame_override)
else()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/egame)
endif()

file(GLOB_RECURSE SOURCE_FILES Src/*.cpp Src/*.hpp Src/*.inl Protobuf/Build/*.cc)

if (NOT ${IOMOMI_EDITOR})
	list(FILTER SOURCE_FILES EXCLUDE REGEX "Src/Editor/.*")
endif()

add_executable(iomomi ${SOURCE_FILES})

target_precompile_headers(iomomi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Src/PCH.hpp)

target_compile_options(iomomi PRIVATE
	-Wall
	-Wextra
	-Wshadow
	-Wcast-qual
	-Wfloat-conversion
	-Woverlength-strings
	-Wpointer-arith
	-Wunused-local-typedefs
	-Wunused-result
	-Wvarargs
	-Wvla
	-Wstrict-aliasing
	-Wwrite-strings
	-pedantic
	-Wno-unused-parameter
	-Wno-delete-non-virtual-dtor
	-Wno-missing-field-initializers
	$<$<COMPILE_LANGUAGE:CXX>:-Wconversion-null>
)

#Adds compiler specific options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(Src/Graphics/Water/WaterSimulatorImpl.cpp PROPERTIES COMPILE_FLAGS "-O2 -g0")
	set_source_files_properties(Src/Graphics/Water/WaterSimulatorImplAvx2.cpp PROPERTIES COMPILE_FLAGS "-O2 -g0 -mavx2")
	set_source_files_properties(Src/Graphics/Water/WaterSimulatorImplAvx512.cpp PROPERTIES COMPILE_FLAGS "-O2 -g0 -mavx512f")
	set_source_files_properties(Src/World/Collision.cpp PROPERTIES COMPILE_FLAGS "-O2 -g0")
endif()

target_link_libraries(iomomi EGame)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(iomomi PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		INSTALL_RPATH "$ORIGIN/rt"
		BUILD_WITH_INSTALL_RPATH TRUE)
	set_target_properties(EGame EGameAssetGen PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}/rt
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_link_options(iomomi PRIVATE "-Wl,-subsystem,windows")
endif()

set_target_properties(iomomi PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME}
	OUTPUT_NAME "iomomi"
	LINKER_LANGUAGE CXX
	CXX_STANDARD 20
)

string(TIMESTAMP BUILD_DATE "%d-%m-%Y")
target_compile_options(iomomi PRIVATE -DBUILD_DATE="${BUILD_DATE}")

if(NOT ${BUILD_ID} STREQUAL "")
	target_compile_options(iomomi PRIVATE -DBUILD_ID="${BUILD_ID}")
endif()

target_include_directories(iomomi SYSTEM PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Inc
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/pcg/include
	${CMAKE_CURRENT_SOURCE_DIR}/Deps/magic_enum/include
)

# flags for editor
if (${IOMOMI_EDITOR})
	target_link_libraries(iomomi EGameImGui)
	target_compile_definitions(iomomi PRIVATE -DIOMOMI_ENABLE_EDITOR)
endif()

if (${IOMOMI_WATER})
	target_compile_definitions(iomomi PRIVATE -DIOMOMI_ENABLE_WATER)
endif()

# finds and adds protobuf
find_package(Protobuf CONFIG REQUIRED)
target_link_libraries(iomomi protobuf::libprotobuf)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	string(CONCAT EMCC_FLAGS
		"-lidbfs.js -lopenal -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s FORCE_FILESYSTEM=1 -s FETCH=1 "
		"-s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_ZLIB=1 -s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=0")
	
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		set(EMCC_FLAGS "${EMCC_FLAGS} -g4")
		target_compile_options(iomomi PRIVATE -gsource-map)
	endif()
	
	target_compile_options(iomomi PRIVATE -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-mismatched-tags)
	set_target_properties(iomomi PROPERTIES LINK_FLAGS "-s EXPORTED_RUNTIME_METHODS=['cwrap'] ${EMCC_FLAGS}")
else()
	target_link_libraries(iomomi stdc++fs SDL2)
endif()
