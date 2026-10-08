# Runs sw_battle_test on one scenario and compares its stdout with the recorded expected output.
# Plain CMake script mode, so it behaves the same on Windows, Linux and macOS.
#
#   cmake -DBINARY=<sw_battle_test> -DSCENARIO=<name.txt> -DEXPECTED=<name.expected> -DSEED=<n>
#         [-DUPDATE=ON] -P run_scenario.cmake
#
# UPDATE=ON records the current output as the expected one instead of comparing (see tests/CMakeLists.txt for the
# update_expected targets that drive this).

foreach(variable BINARY SCENARIO EXPECTED SEED)
	if(NOT DEFINED ${variable})
		message(FATAL_ERROR "run_scenario.cmake: -D${variable}=... is required")
	endif()
endforeach()

execute_process(
	COMMAND "${BINARY}" "${SCENARIO}" --seed "${SEED}"
	OUTPUT_VARIABLE actual
	ERROR_VARIABLE errors
	RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "sw_battle_test exited with '${result}' on ${SCENARIO}\n${errors}")
endif()

# std::cout in text mode writes \r\n on Windows; the recorded output always uses \n.
string(REPLACE "\r\n" "\n" actual "${actual}")

cmake_path(GET EXPECTED FILENAME expected_name)
cmake_path(GET SCENARIO STEM scenario_name)

if(UPDATE)
	file(WRITE "${EXPECTED}" "${actual}")
	message(STATUS "Recorded ${expected_name} (seed ${SEED})")
	return()
endif()

# No recorded output yet: CTest reports the test as skipped (SKIP_REGULAR_EXPRESSION in tests/CMakeLists.txt).
if(NOT EXISTS "${EXPECTED}")
	message("SW_E2E_SKIP: no expected output for '${scenario_name}' yet (${EXPECTED}).\n"
		"Record it with:  cmake --build --preset debug --target update_expected_${scenario_name}")
	return()
endif()

file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(actual STREQUAL expected)
	return()
endif()

# Find the first differing line. Walked by hand: the output contains [brackets], which CMake lists treat specially.
set(line 1)
set(rest_expected "${expected}")
set(rest_actual "${actual}")
while(TRUE)
	string(FIND "${rest_expected}" "\n" expected_end)
	string(FIND "${rest_actual}" "\n" actual_end)
	if(expected_end EQUAL -1)
		set(expected_line "${rest_expected}")
	else()
		string(SUBSTRING "${rest_expected}" 0 ${expected_end} expected_line)
	endif()
	if(actual_end EQUAL -1)
		set(actual_line "${rest_actual}")
	else()
		string(SUBSTRING "${rest_actual}" 0 ${actual_end} actual_line)
	endif()

	if(NOT expected_line STREQUAL actual_line OR expected_end EQUAL -1 OR actual_end EQUAL -1)
		break()
	endif()
	math(EXPR expected_end "${expected_end} + 1")
	math(EXPR actual_end "${actual_end} + 1")
	string(SUBSTRING "${rest_expected}" ${expected_end} -1 rest_expected)
	string(SUBSTRING "${rest_actual}" ${actual_end} -1 rest_actual)
	math(EXPR line "${line} + 1")
endwhile()

if(expected_line STREQUAL "" AND expected_end EQUAL -1)
	set(expected_line "<end of output>")
endif()
if(actual_line STREQUAL "" AND actual_end EQUAL -1)
	set(actual_line "<end of output>")
endif()

# Keep the full actual output next to the build so it can be diffed against the expected file.
set(actual_file "${CMAKE_CURRENT_BINARY_DIR}/${scenario_name}.actual")
file(WRITE "${actual_file}" "${actual}")

message(FATAL_ERROR
	"Output of '${scenario_name}' differs from ${expected_name}, first at line ${line}:\n"
	"  expected: ${expected_line}\n"
	"  actual:   ${actual_line}\n"
	"Full output: ${actual_file}\n"
	"Compare:     git diff --no-index \"${EXPECTED}\" \"${actual_file}\"\n"
	"If the new output is correct, record it with:\n"
	"  cmake --build --preset debug --target update_expected_${scenario_name}")
