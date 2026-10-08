cmake_minimum_required(VERSION 3.27)
project(sw_battle_test LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Needed by clang-tidy (and IDEs / clangd).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Zero-warning policy: every warning is an error. Override with -DCMAKE_COMPILE_WARNING_AS_ERROR=OFF
# (or `cmake --compile-no-warning-as-error`) only for local experiments.
set(CMAKE_COMPILE_WARNING_AS_ERROR ON CACHE BOOL "Treat compiler warnings as errors")

option(SW_ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
# Source-based coverage for `tools/scripts/check.py coverage` (the `coverage` preset turns it on). Clang only.
option(SW_ENABLE_COVERAGE "Build with Clang source-based coverage instrumentation" OFF)
# libFuzzer builds of tests/fuzz/ (`tools/scripts/check.py fuzz` turns it on, with LLVM's Clang). The whole engine is
# instrumented for coverage feedback and checked with ASan + UBSan.
option(SW_ENABLE_FUZZING "Build the fuzz targets with libFuzzer (LLVM Clang)" OFF)

# Compiler options shared by every target of the project (engine, executable, unit tests).
add_library(sw_build_options INTERFACE)

if(MSVC)
	target_compile_options(sw_build_options INTERFACE /W4 /permissive- /sdl /guard:cf)
	target_link_options(sw_build_options INTERFACE /guard:cf)
else()
	target_compile_options(sw_build_options INTERFACE
		-Wall
		-Wextra
		-Wpedantic
		-pedantic-errors
		-Wshadow
		-Wconversion
		-Wsign-conversion
		-Wold-style-cast
		-Wcast-align
		-Wcast-qual
		-Wnon-virtual-dtor
		-Woverloaded-virtual
		-Wnull-dereference
		-Wdouble-promotion
		-Wformat=2
		-Wimplicit-fallthrough
		-Wmissing-declarations
		-Wundef
		$<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast>
		# Clang's -Wshadow skips constructor parameters and lambda captures; -Wshadow-all matches GCC's -Wshadow.
		$<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wshadow-all -Wextra-semi -Wunreachable-code -Wcomma>
		# Doxygen comments must match the code: \param names, \return on void functions, unknown commands.
		$<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wdocumentation>

		# Hardening
		-fstack-protector-strong
		# Not with sanitizers: ASan defines _FORTIFY_SOURCE=0 itself and does that checking better.
		$<$<AND:$<NOT:$<CONFIG:Debug>>,$<NOT:$<BOOL:${SW_ENABLE_SANITIZERS}>>,$<NOT:$<BOOL:${SW_ENABLE_FUZZING}>>>:-D_FORTIFY_SOURCE=3>
		$<$<CXX_COMPILER_ID:GNU>:-D_GLIBCXX_ASSERTIONS>
		$<$<CXX_COMPILER_ID:Clang,AppleClang>:-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE>
	)

	if(SW_ENABLE_COVERAGE)
		if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			message(FATAL_ERROR "SW_ENABLE_COVERAGE needs Clang (source-based coverage)")
		endif()
		target_compile_options(sw_build_options INTERFACE -fprofile-instr-generate -fcoverage-mapping)
		target_link_options(sw_build_options INTERFACE -fprofile-instr-generate)
	endif()

	if(SW_ENABLE_FUZZING)
		if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			message(FATAL_ERROR "SW_ENABLE_FUZZING needs LLVM's Clang (AppleClang ships no libFuzzer runtime)")
		endif()
		target_compile_options(sw_build_options INTERFACE -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
		target_link_options(sw_build_options INTERFACE -fsanitize=address,undefined)
	endif()

	if(SW_ENABLE_SANITIZERS)
		target_compile_options(sw_build_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
		target_link_options(sw_build_options INTERFACE -fsanitize=address,undefined)
	endif()
endif()

if(APPLE)
	# AppleClang locates the macOS SDK implicitly, so CMake leaves -isysroot out of the
	# compile line. Tools that replay compile_commands.json (Homebrew's clang-tidy,
	# clangd) are not AppleClang: without the SDK path they cannot find the standard
	# library headers and report nonsense. Record it explicitly.
	execute_process(
		COMMAND xcrun --show-sdk-path
		OUTPUT_VARIABLE SW_MACOS_SDK_PATH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	if(SW_MACOS_SDK_PATH)
		target_compile_options(sw_build_options INTERFACE -isysroot ${SW_MACOS_SDK_PATH})
	else()
		message(WARNING "xcrun --show-sdk-path failed; clang-tidy may not find system headers")
	endif()
endif()

# Core/ is the simulation engine, Features/ holds per-unit mechanics. Everything but main.cpp is a library, so the
# unit tests (tests/unit/) link the same code the executable runs.
file(GLOB_RECURSE SW_CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.hpp)
list(FILTER SW_CORE_SOURCES EXCLUDE REGEX "/src/main\\.cpp$")
add_library(sw_core STATIC ${SW_CORE_SOURCES})
target_link_libraries(sw_core PUBLIC sw_build_options)

include(FetchContent)
# Boost.Describe and Boost.Mp11, standalone: header-only reflection from one declaration per type (SW_REFLECT,
# Core/Base/Reflect.hpp). The engine walks a unit's kit (events), a command's fields (parsing) and a log record's
# fields (printing) through it. Fetched at configure time and pinned by hash, like doctest.
FetchContent_Declare(boost_describe
	URL https://github.com/boostorg/describe/archive/refs/tags/boost-1.87.0.tar.gz
	URL_HASH SHA256=6f377d72544f31c4feeae9342b5db79b2436f60f67123ef9f3a4b4f760bace10
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	# Headers only: their CMakeLists target the Boost superproject.
	SOURCE_SUBDIR headers-only
)
FetchContent_Declare(boost_mp11
	URL https://github.com/boostorg/mp11/archive/refs/tags/boost-1.87.0.tar.gz
	URL_HASH SHA256=f07a7282a4b440ff9f2917224badd4c112affede946ae25fba6b3e3538a858de
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SOURCE_SUBDIR headers-only
)
FetchContent_MakeAvailable(boost_describe boost_mp11)
add_library(sw_describe INTERFACE)
target_include_directories(sw_describe SYSTEM INTERFACE
	"${boost_describe_SOURCE_DIR}/include" "${boost_mp11_SOURCE_DIR}/include")
target_link_libraries(sw_core PUBLIC sw_describe)

# src/ is the include root, so project headers are spelled in full from there:
# <Core/Game/World.hpp>, <Features/Hunter/Hunter.hpp>. src/Features/ is a second
# root purely so a feature can refer to its own headers by the short
# <Hunter/Ability.hpp> form; the sw-include-style clang-tidy check keeps that form
# inside the feature that owns it.
target_include_directories(sw_core PUBLIC src/ src/Features/)

# Every header compiled on its own: proves it is self-contained (includes what it uses), and gives clang-tidy a
# translation unit for headers that no .cpp in src/ includes yet (e.g. a component only features will use), so
# `check.py tidy --all` sees every header. Not linked into anything.
set(SW_HEADER_CHECK_DIR "${CMAKE_BINARY_DIR}/header-check")
set(SW_HEADER_CHECK_SOURCES)
foreach(header IN LISTS SW_CORE_SOURCES)
	if(header MATCHES "\\.hpp$")
		file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}/src" "${header}")
		set(check "${SW_HEADER_CHECK_DIR}/${relative}.cpp")
		file(CONFIGURE OUTPUT "${check}" CONTENT "#include <${relative}>  // IWYU pragma: keep\n" @ONLY)
		list(APPEND SW_HEADER_CHECK_SOURCES "${check}")
	endif()
endforeach()
add_library(sw_header_check OBJECT ${SW_HEADER_CHECK_SOURCES})
target_link_libraries(sw_header_check PRIVATE sw_core)

add_executable(sw_battle_test src/main.cpp)
target_link_libraries(sw_battle_test PRIVATE sw_core)

# Quoted and relative includes resolve against the including file's own
# directory, which no -I setting can turn off, so the spelling rules are a
# clang-tidy check (sw-include-style) in this plugin, run by `tools/scripts/check.py tidy`.
option(SW_BUILD_TIDY_PLUGIN "Define the sw_tidy_checks clang-tidy plugin target (needs LLVM's CMake package)" ON)
if(SW_BUILD_TIDY_PLUGIN)
	add_subdirectory(tools/clang-tidy-checks)
endif()

set_target_properties(sw_battle_test PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT sw_battle_test)

# Tests (details in tests/CMakeLists.txt):
#   unit/  doctest unit tests of sw_core (sw_unit_tests); each TEST_CASE is its own CTest test.
#   e2e/   each scenario in tests/e2e/scenarios/ must reproduce tests/e2e/.expected/<name>.expected.
#   ctest --preset debug                                            run them all
#   cmake --build --preset debug --target update_expected           re-record all expected e2e output
#   cmake --build --preset debug --target update_expected_<name>    record one scenario (creates the file)
enable_testing()
add_subdirectory(tests)
