# Unit tests of sw_core with doctest (https://github.com/doctest/doctest), fetched at configure time and pinned by
# hash. Every TEST_CASE is registered as its own CTest test (unit.<name>), so `ctest -R unit` runs them and failures
# are reported per case.
#
# The folders mirror src/: tests/unit/Core/Simulation/ tests src/Core/Simulation/, and so on. Support/ holds the test
# doubles and fixtures (scripted randomness, a recording battle log, test units, TestWorld).
#
#   ctest --preset debug -L unit                       all unit tests
#   ./build/debug/tests/unit/Debug/sw_unit_tests -h    doctest's own runner and options (filters, -s for successes)

include(FetchContent)

set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
# The tests provide their own main (main.cpp); doctest's prebuilt one would be compiled under our warning flags.
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)

FetchContent_Declare(doctest
	URL https://github.com/doctest/doctest/archive/refs/tags/v2.5.3.tar.gz
	URL_HASH SHA256=174ebc4e769928959614789c5b4e9c3d0a0f81a62bb608756b127bfebfb21331
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	SYSTEM  # third-party headers are exempt from the project's warnings-as-errors policy
)
FetchContent_MakeAvailable(doctest)
include("${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake")

file(GLOB_RECURSE SW_UNIT_TEST_SOURCES CONFIGURE_DEPENDS *.cpp *.hpp)
add_executable(sw_unit_tests ${SW_UNIT_TEST_SOURCES})
target_link_libraries(sw_unit_tests PRIVATE sw_core sw_test_options doctest::doctest)
# doctest prints std::string_view through <ostream>, which MSVC's headers do not include on their own.
target_compile_definitions(sw_unit_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
# Support headers are included as <Support/...>.
target_include_directories(sw_unit_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

doctest_discover_tests(sw_unit_tests TEST_PREFIX "unit." PROPERTIES LABELS unit)
