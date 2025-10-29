cmake_minimum_required(VERSION 3.20)
project(Adria VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_RELEASE} /Zi" CACHE STRING "Flags for Profile build")
    set(CMAKE_EXE_LINKER_FLAGS_PROFILE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF" CACHE STRING "Linker flags for Profile build")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Profile;Release;RelWithDebInfo" CACHE STRING "Available build types" FORCE)
endif()

if(MSVC)
    add_compile_options(/MP)
    add_compile_definitions(
        UNICODE
        _UNICODE
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
		_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
		_SILENCE_CXX20_CISO646_REMOVED_WARNING
		WIN32_LEAN_AND_MEAN
		"SOLUTION_DIR=R\"(${CMAKE_SOURCE_DIR})\""
		"SOLUTION_DIR_W=LR\"(${CMAKE_SOURCE_DIR})\""
    )
endif()

if(APPLE)
    add_compile_definitions(
        "SOLUTION_DIR=R\"(${CMAKE_SOURCE_DIR})\""
		"SOLUTION_DIR_W=LR\"(${CMAKE_SOURCE_DIR})\""
		PAL_STDCPP_COMPAT  # Prevent sal.h from redefining __null
    )
    # Enable Objective-C++ support
    enable_language(OBJCXX)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(Adria)