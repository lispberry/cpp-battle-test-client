# Fuzz tests: one executable per file in Targets/, each a typed function wrapped by SW_FUZZ_TARGET (Support/Target.hpp).
# Fuzzer bytes are decoded with LLVM's FuzzedDataProvider into game objects (Support/GameObjects.hpp).
#
# Two ways to build them:
#   - libFuzzer (SW_ENABLE_FUZZING=ON, LLVM Clang with its fuzzer runtime): real coverage-guided fuzzing.
#     `uv run tools/scripts/check.py fuzz <target>` configures build/fuzz and runs it.
#   - replay (default, any compiler): Replay.cpp provides main(); CTest runs each target on its corpus in
#     tests/fuzz/corpus/<target>/ (inputs libFuzzer found, kept as regressions) plus deterministic generated inputs.

# The header comes with Clang (AppleClang and LLVM); GCC and MSVC get the same file from the LLVM release, pinned.
include(CheckIncludeFileCXX)
check_include_file_cxx(fuzzer/FuzzedDataProvider.h SW_HAVE_FUZZED_DATA_PROVIDER)
add_library(sw_fuzzed_data_provider INTERFACE)
if(NOT SW_HAVE_FUZZED_DATA_PROVIDER)
	set(provider "${CMAKE_BINARY_DIR}/_deps/fuzzed_data_provider/include/fuzzer/FuzzedDataProvider.h")
	file(DOWNLOAD
		https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-21.1.8/compiler-rt/include/fuzzer/FuzzedDataProvider.h
		"${provider}"
		EXPECTED_HASH SHA256=895dc2aafb7ab1583c13d18b86e661960f24d87f4f48e54f98fa84aac410aa86
	)
	target_include_directories(sw_fuzzed_data_provider SYSTEM INTERFACE "${CMAKE_BINARY_DIR}/_deps/fuzzed_data_provider/include")
endif()

set(SW_FUZZ_GENERATED_RUNS 300 CACHE STRING "Generated inputs each fuzz target replays under CTest")

# Replay.cpp saves a crashing input with std::fopen from a signal handler, where MSVC's fopen_s is no better.
function(sw_replay_driver_options target)
	if(MSVC)
		target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
	endif()
endfunction()

file(GLOB targets CONFIGURE_DEPENDS Targets/*.cpp)
foreach(source IN LISTS targets)
	cmake_path(GET source STEM name)
	set(target fuzz_${name})
	add_executable(${target} ${source})
	target_link_libraries(${target} PRIVATE sw_core sw_test_options sw_fuzzed_data_provider)
	# Support headers are included as <Support/...>.
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	if(SW_ENABLE_FUZZING)
		target_link_options(${target} PRIVATE -fsanitize=fuzzer)
	else()
		target_sources(${target} PRIVATE Replay.cpp)
		sw_replay_driver_options(${target})
		add_test(NAME fuzz.${name}
			COMMAND ${target} "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}" --generated ${SW_FUZZ_GENERATED_RUNS})
		set_tests_properties(fuzz.${name} PROPERTIES LABELS fuzz)
	endif()
endforeach()

# The replay driver itself, tested with a probe target that has a known bug (ReplayProbe/probe.cpp). Only in replay
# builds: under libFuzzer the driver is not linked.
if(NOT SW_ENABLE_FUZZING)
	add_executable(fuzz_replay_probe ReplayProbe/probe.cpp Replay.cpp)
	target_link_libraries(fuzz_replay_probe PRIVATE sw_core sw_test_options sw_fuzzed_data_provider)
	target_include_directories(fuzz_replay_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	sw_replay_driver_options(fuzz_replay_probe)

	# sw_replay_test(<name> COMMAND <args...> EXIT <code|nonzero> OUTPUT <regex> [CLEAN <dir>])
	# Runs the probe with <args> through ReplayProbe/run.cmake, which checks the exit code and the output.
	function(sw_replay_test name)
		cmake_parse_arguments(PARSE_ARGV 1 arg "" "EXIT;OUTPUT;CLEAN" "COMMAND")
		list(JOIN arg_COMMAND "|" command)  # run.cmake splits it again; a ';' would not survive add_test
		set(defines "-DCOMMAND=$<TARGET_FILE:fuzz_replay_probe>|${command}" "-DEXPECT_EXIT=${arg_EXIT}"
			"-DEXPECT_OUTPUT=${arg_OUTPUT}")
		if(arg_CLEAN)
			list(APPEND defines "-DCLEAN_DIR=${arg_CLEAN}")
		endif()
		add_test(NAME fuzz.replay.${name}
			COMMAND ${CMAKE_COMMAND} ${defines} -P "${CMAKE_CURRENT_SOURCE_DIR}/ReplayProbe/run.cmake")
		set_tests_properties(fuzz.replay.${name} PROPERTIES LABELS "fuzz;replay")
	endfunction()

	set(inputs "${CMAKE_CURRENT_SOURCE_DIR}/ReplayProbe/inputs")
	set(crashes "${CMAKE_CURRENT_BINARY_DIR}/replay-crashes")

	sw_replay_test(passes_a_good_file COMMAND "${inputs}/a-fine.txt"
		EXIT 0 OUTPUT "replayed 1 corpus files and 0 generated inputs")
	sw_replay_test(fails_on_a_bad_file COMMAND "${inputs}/b-crash.txt"
		EXIT nonzero OUTPUT "no input starts with CRASH.*broke the target: .*b-crash.txt.*replay it with: ")
	sw_replay_test(replays_a_directory_in_name_order COMMAND "${inputs}"
		EXIT nonzero OUTPUT "broke the target: .*b-crash.txt")
	sw_replay_test(skips_missing_paths COMMAND "${inputs}/missing" "${inputs}/a-fine.txt"
		EXIT 0 OUTPUT "skipping .*missing.*replayed 1 corpus files")
	# The generated inputs are the same everywhere: the probe's 0xAB bug is first reached by input #23.
	sw_replay_test(passes_generated_inputs_without_bugs COMMAND --generated 20
		EXIT 0 OUTPUT "replayed 0 corpus files and 20 generated inputs")
	sw_replay_test(rejects_a_bad_count COMMAND --generated x EXIT 2 OUTPUT "usage:")
	sw_replay_test(rejects_a_missing_count COMMAND --generated EXIT 2 OUTPUT "usage:")
	sw_replay_test(rejects_an_unknown_option COMMAND --bogus EXIT 2 OUTPUT "usage:")
	sw_replay_test(saves_a_generated_crash COMMAND --generated 3000 --crash-dir "${crashes}" CLEAN "${crashes}"
		EXIT nonzero OUTPUT "no input starts with byte 0xAB.*broke the target: generated input #23.*saved to .*crash-23")
	sw_replay_test(reproduces_the_saved_crash COMMAND "${crashes}/crash-23"
		EXIT nonzero OUTPUT "no input starts with byte 0xAB.*broke the target: .*crash-23")
	set_tests_properties(fuzz.replay.saves_a_generated_crash PROPERTIES FIXTURES_SETUP replay_crash)
	set_tests_properties(fuzz.replay.reproduces_the_saved_crash PROPERTIES FIXTURES_REQUIRED replay_crash)
endif()
