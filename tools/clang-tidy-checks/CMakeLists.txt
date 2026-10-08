# clang-tidy plugin with the project's own checks (sw-include-style, sw-constrained-templates, sw-documented-api). Not part of the normal build:
# `tools/scripts/check.py tidy` builds the sw_tidy_checks target and loads it with --load.
#
# The plugin borrows clang-tidy's symbols at load time, so it must be compiled against the headers of the exact LLVM
# that runs it. check.py passes the clang-tidy it uses in as SW_CLANG_TIDY; the LLVM CMake package is looked up next
# to that binary.

if(WIN32)
	message(STATUS "clang-tidy plugin: skipped, clang-tidy cannot load plugins on Windows")
	return()
endif()

find_program(SW_CLANG_TIDY
	NAMES clang-tidy clang-tidy-23
	HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin /usr/lib/llvm-23/bin
	DOC "clang-tidy the plugin is built for; its LLVM headers are used"
)
if(NOT SW_CLANG_TIDY)
	message(STATUS "clang-tidy plugin: skipped, clang-tidy not found")
	return()
endif()

# Homebrew's /opt/homebrew/opt/llvm is a symlink that follows upgrades, so look next to the unresolved path first
# and keep the cached Clang_DIR valid across `brew upgrade llvm`.
cmake_path(GET SW_CLANG_TIDY PARENT_PATH _sw_llvm_bin)
cmake_path(GET _sw_llvm_bin PARENT_PATH _sw_llvm_prefix)
file(REAL_PATH "${SW_CLANG_TIDY}" _sw_clang_tidy_real)
cmake_path(GET _sw_clang_tidy_real PARENT_PATH _sw_llvm_bin_real)
cmake_path(GET _sw_llvm_bin_real PARENT_PATH _sw_llvm_prefix_real)

# Distribution packages often ship ClangConfig.cmake without the clang-tidy headers or the libraries it refers to
# (Ubuntu's llvm-18 does), and find_package then fails the whole configure even with QUIET. Only look for the
# package where everything the plugin needs is installed.
set(_sw_clang_dir "")
foreach(prefix IN ITEMS "${_sw_llvm_prefix}" "${_sw_llvm_prefix_real}")
	if(EXISTS "${prefix}/lib/cmake/clang/ClangConfig.cmake"
			AND EXISTS "${prefix}/include/clang-tidy/ClangTidyCheck.h"
			AND EXISTS "${prefix}/lib/libclangBasic.a")
		set(_sw_clang_dir "${prefix}/lib/cmake/clang")
		break()
	endif()
endforeach()
if(NOT _sw_clang_dir)
	message(STATUS "clang-tidy plugin: skipped, no clang-tidy development files next to ${SW_CLANG_TIDY}")
	return()
endif()

# LLVMConfig probes for optional dependencies with check_include_file, which compiles C.
enable_language(C)
find_package(Clang CONFIG QUIET HINTS "${_sw_clang_dir}" NO_DEFAULT_PATH)
if(NOT Clang_FOUND)
	message(STATUS "clang-tidy plugin: skipped, no LLVM CMake package next to ${SW_CLANG_TIDY}")
	return()
endif()
message(STATUS "clang-tidy plugin: building against LLVM ${LLVM_PACKAGE_VERSION} for ${SW_CLANG_TIDY}")

add_library(sw_tidy_checks MODULE EXCLUDE_FROM_ALL
	SwTidyModule.cpp
	IncludeStyleCheck.cpp
	ConstrainedTemplatesCheck.cpp
	DocumentedApiCheck.cpp
)

# SYSTEM keeps LLVM's headers out of the project's warnings-as-errors policy.
target_include_directories(sw_tidy_checks SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
separate_arguments(_sw_llvm_definitions NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(sw_tidy_checks PRIVATE ${_sw_llvm_definitions} $<$<NOT:$<BOOL:${LLVM_ENABLE_RTTI}>>:-fno-rtti>)

# Leave clang-tidy's own symbols unresolved; they come from the binary that loads the plugin. Linking libclangTidy
# in would register every built-in check a second time.
if(APPLE)
	target_link_options(sw_tidy_checks PRIVATE -undefined dynamic_lookup)
endif()

# A fixed path (no lib prefix, no per-config subdirectory) so check.py can find it: <build>/SwTidyChecks.so
set_target_properties(sw_tidy_checks PROPERTIES
	OUTPUT_NAME SwTidyChecks
	PREFIX ""
	SUFFIX ".so"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>"
)
