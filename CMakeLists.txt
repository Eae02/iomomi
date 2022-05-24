cmake_minimum_required(VERSION 3.16)
project(iomomi)

set(BUILD_DIR_NAME ${CMAKE_BUILD_TYPE})

set(BUILD_ID "" CACHE STRING "Build id to be included in the main menu.")

set(BUILD_ASSETMAN OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Deps/egame_override")
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/egame_override)
else()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/egame)
endif()

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

target_compile_options(iomomi PRIVATE -Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-delete-non-virtual-dtor -Wno-missing-field-initializers)

#Adds compiler specific options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(Src/Graphics/Water/WaterSimulatorImpl.cpp PROPERTIES COMPILE_FLAGS "-O2 -g0")
#	set_source_files_properties(Src/Graphics/Water/WaterSimulatorWorker.cpp PROPERTIES COMPILE_FLAGS "-O2 -g0")
	set_source_files_properties(Src/World/Collision.cpp PROPERTIES COMPILE_FLAGS "-O2 -g0")
endif()

target_link_libraries(iomomi EGame)

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
	LINKER_LANGUAGE CXX
	CXX_STANDARD 20
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

if (${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	set(EMCC_FLAGS "-lidbfs.js -lopenal -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_ZLIB=1 -s FORCE_FILESYSTEM=1 -s FETCH=1 -s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=0")
	
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		set(EMCC_FLAGS "${EMCC_FLAGS} -g4 --source-map-base='http://0.0.0.0:7000/'")
		target_compile_options(iomomi PRIVATE -gsource-map)
	endif()
	
	target_link_libraries(iomomi "${PROTOBUF_SRC_DIR}/.libs/libprotobuf.a")
	target_include_directories(iomomi SYSTEM PRIVATE ${PROTOBUF_SRC_DIR})
	
	target_compile_options(iomomi PRIVATE
		-Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-mismatched-tags
		-DIOMOMI_NO_WATER
		-DIOMOMI_NO_EDITOR
		)
	set_target_properties(iomomi PROPERTIES LINK_FLAGS "-s EXPORTED_RUNTIME_METHODS=['cwrap'] -s EXPORTED_FUNCTIONS=['_WebMain'] ${EMCC_FLAGS}")
else()
	find_package(Protobuf REQUIRED)
	fixIncludes(protobuf::libprotobuf)
	
	find_package(PkgConfig)
	pkg_check_modules(TBB REQUIRED tbb)
	
	target_link_libraries(iomomi protobuf::libprotobuf stdc++fs ${TBB_LIBRARIES})
	target_include_directories(iomomi PRIVATE ${TBB_INCLUDE_DIRS})
	target_compile_options(iomomi PRIVATE ${TBB_CFLAGS_OTHER})
	
	target_compile_options(iomomi PRIVATE -msse4.1)
endif()
