﻿cmake_minimum_required(VERSION 3.15)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project(pngsquish)

if (MSVC)
	option(PNGSQUISH_CRT_STATIC "Statically link the MSVC runtime library" ON)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<NOT:$<BOOL:${PNGSQUISH_CRT_STATIC}>>:DLL>")
endif()

add_executable(pngsquish
	src/main.cpp
	src/random.cpp
	src/image.cpp
	src/buffer.cpp
	src/file.cpp
	src/perspective.cpp
	src/gui.cpp
	src/stb_image.cpp
	src/glad.c
)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 20)

add_subdirectory(lib)

target_include_directories(${CMAKE_PROJECT_NAME}
	PRIVATE inc
	PUBLIC lib
	PUBLIC lib/imgui
	PUBLIC lib/glfw/include
)

target_link_libraries(${CMAKE_PROJECT_NAME}
	libdeflate::libdeflate_static
	glfw
	imgui
	nfd
)
