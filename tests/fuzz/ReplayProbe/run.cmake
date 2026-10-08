# Runs a command and checks how it ended: CTest's WILL_FAIL cannot also check what the output says.
#
#   cmake -DCOMMAND=<program|arg|arg...> [-DCLEAN_DIR=<dir>] [-DEXPECT_EXIT=<code|nonzero>] [-DEXPECT_OUTPUT=<regex>] -P run.cmake

if(NOT DEFINED COMMAND)
	message(FATAL_ERROR "run.cmake: -DCOMMAND=... is required")
endif()

string(REPLACE "|" ";" COMMAND "${COMMAND}")

if(DEFINED CLEAN_DIR)
	file(REMOVE_RECURSE "${CLEAN_DIR}")
	file(MAKE_DIRECTORY "${CLEAN_DIR}")
endif()

# One variable for both streams: CMake then keeps stdout and stderr interleaved in the order they were written.
execute_process(COMMAND ${COMMAND} OUTPUT_VARIABLE all ERROR_VARIABLE all RESULT_VARIABLE result)

if(DEFINED EXPECT_EXIT)
	if(EXPECT_EXIT STREQUAL "nonzero")
		if(result EQUAL 0)
			message(FATAL_ERROR "expected a failure, the command succeeded:\n${all}")
		endif()
	elseif(NOT result STREQUAL EXPECT_EXIT)
		message(FATAL_ERROR "expected exit '${EXPECT_EXIT}', got '${result}':\n${all}")
	endif()
endif()

if(DEFINED EXPECT_OUTPUT AND NOT all MATCHES "${EXPECT_OUTPUT}")
	message(FATAL_ERROR "output does not match '${EXPECT_OUTPUT}':\n${all}")
endif()
