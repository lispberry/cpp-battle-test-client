# Tests state only the fields that matter (`{.health = {Hp{3}}}`) and rely on defaults for the rest, which newer
# Clang reports as -Wmissing-designated-field-initializers. The engine in src/ keeps the warning; tests opt out.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wno-missing-designated-field-initializers SW_HAVE_NO_MISSING_DESIGNATED)
add_library(sw_test_options INTERFACE)
if(SW_HAVE_NO_MISSING_DESIGNATED)
	target_compile_options(sw_test_options INTERFACE -Wno-missing-designated-field-initializers)
endif()
# GCC reports the same under -Wmissing-field-initializers (and accepts any -Wno-* flag, so the check above passes).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(sw_test_options INTERFACE -Wno-missing-field-initializers)
endif()

# unit/: doctest unit tests of sw_core, see unit/CMakeLists.txt.
add_subdirectory(unit)

# fuzz/: libFuzzer targets; replayed under CTest (label "fuzz") unless built for fuzzing, see fuzz/CMakeLists.txt.
add_subdirectory(fuzz)

# End-to-end tests: every tests/e2e/scenarios/<name>.txt is run through sw_battle_test with a fixed --seed and its
# stdout must match tests/e2e/.expected/<name>.expected byte for byte (line endings aside).
#
#   ctest --preset debug                    run all of them
#   ctest --preset debug -R swordsman       run the ones matching a pattern
#
# Recreating expected output (after an intended change in behaviour; review the diff before committing):
#
#   cmake --build --preset debug --target update_expected            re-record every existing .expected file
#   cmake --build --preset debug --target update_expected_<name>     record one scenario, creating the file if needed
#
# update_expected only refreshes files that already exist, so it never records output for a scenario nobody has
# checked yet. A scenario without a .expected file is reported as skipped; record it with its own target once the
# behaviour it covers is implemented (hunter_poison waits for the Hunter).
#
# Adding a scenario: drop <name>.txt into tests/e2e/scenarios/, re-run cmake, build update_expected_<name>.

# All scenarios share one seed, so recorded output is reproducible on every platform (MersenneRandom is portable).
set(SW_E2E_SEED 42 CACHE STRING "--seed passed to sw_battle_test by the e2e tests")

find_package(Python3 COMPONENTS Interpreter)
set(e2e_dir "${CMAKE_CURRENT_SOURCE_DIR}/e2e")
set(run_scenario "${e2e_dir}/run_scenario.cmake")
file(GLOB scenarios CONFIGURE_DEPENDS "${e2e_dir}/scenarios/*.txt")
# Globbed too, so recording a new file re-runs cmake on the next build and update_expected picks it up.
file(GLOB recorded CONFIGURE_DEPENDS "${e2e_dir}/.expected/*.expected")

add_custom_target(update_expected
	COMMENT "Re-recording existing e2e expected output"
)

foreach(scenario IN LISTS scenarios)
	cmake_path(GET scenario STEM name)
	set(expected "${e2e_dir}/.expected/${name}.expected")
	set(run_args
		-DBINARY=$<TARGET_FILE:sw_battle_test>
		-DSCENARIO=${scenario}
		-DEXPECTED=${expected}
		-DSEED=${SW_E2E_SEED}
	)

	add_test(NAME e2e.${name} COMMAND ${CMAKE_COMMAND} ${run_args} -P ${run_scenario})
	set_tests_properties(e2e.${name} PROPERTIES LABELS e2e SKIP_REGULAR_EXPRESSION "SW_E2E_SKIP:")

	add_custom_target(update_expected_${name}
		COMMAND ${CMAKE_COMMAND} ${run_args} -DUPDATE=ON -P ${run_scenario}
		DEPENDS sw_battle_test
		COMMENT "Recording e2e expected output for ${name}"
		VERBATIM
	)
	if(expected IN_LIST recorded)
		add_dependencies(update_expected update_expected_${name})
	endif()

	# The recorded output must follow docs/SPEC.md: e2e.<name> pins the binary to the recording, spec.<name> checks
	# the recording against the rules with an independent model of the game (tests/e2e/spec_check.py).
	if(Python3_Interpreter_FOUND AND expected IN_LIST recorded)
		add_test(NAME spec.${name} COMMAND Python3::Interpreter ${e2e_dir}/spec_check.py ${scenario} ${expected})
		set_tests_properties(spec.${name} PROPERTIES LABELS spec)
	endif()
endforeach()
