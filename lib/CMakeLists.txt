include_directories(
	imgui
	glfw/include
)

set(LIBDEFLATE_BUILD_GZIP OFF CACHE BOOL "Build the libdeflate-gzip program" FORCE)
set(LIBDEFLATE_BUILD_SHARED_LIB OFF CACHE BOOL "Build the shared library" FORCE)
set(LIBDEFLATE_BUILD_STATIC_LIB ON CACHE BOOL "Build the static library" FORCE)
set(LIBDEFLATE_BUILD_TESTS OFF CACHE BOOL "Build the test programs" FORCE)
set(LIBDEFLATE_COMPRESSION_SUPPORT ON CACHE BOOL "Support compression" FORCE)
set(LIBDEFLATE_DECOMPRESSION_SUPPORT OFF CACHE BOOL "Support decompression" FORCE)
set(LIBDEFLATE_FREESTANDING OFF CACHE BOOL "Build a freestanding library, i.e. a library that doesn't link to any
       libc functions like malloc(), free(), and memcpy().  Library users will
       need to provide a custom memory allocator." FORCE)
set(LIBDEFLATE_GZIP_SUPPORT ON CACHE BOOL "Support the gzip format" FORCE)
set(LIBDEFLATE_USE_SHARED_LIB OFF CACHE BOOL "Link the libdeflate-gzip and test programs to the shared library instead
       of the static library" FORCE)
set(LIBDEFLATE_ZLIB_SUPPORT ON CACHE BOOL "Support the zlib format" FORCE)
add_subdirectory(libdeflate)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)
set(GLFW_LIBRARY_TYPE "" CACHE STRING "Library type override for GLFW (SHARED, STATIC, OBJECT, or empty to follow BUILD_SHARED_LIBS)" FORCE)
set(GLFW_USE_HYBRID_HPG OFF CACHE BOOL "Force use of high-performance GPU on hybrid systems" FORCE)
if (MSVC)
	set(USE_MSVC_RUNTIME_LIBRARY_DLL ON CACHE BOOL "Use MSVC runtime library DLL" FORCE)
endif()
add_subdirectory(glfw)

add_library(imgui
	imgui/imgui.cpp
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_glfw.cpp
	imgui/backends/imgui_impl_opengl3.cpp
)
