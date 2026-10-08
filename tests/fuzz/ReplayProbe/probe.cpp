// A fuzz target with a known bug, to test the replay driver itself (tests/fuzz/CMakeLists.txt): its property breaks
// on inputs starting with "CRASH" and on inputs whose first byte is 0xAB (which the generated inputs reach).

#include <Support/Target.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace
{
	void probeHasAKnownBug(const std::string& text)
	{
		sw::fuzz::expect(!text.starts_with("CRASH"), "probe: no input starts with CRASH");
	}
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	sw::fuzz::expect(size == 0 || data[0] != 0xAB, "probe: no input starts with byte 0xAB");
	probeHasAKnownBug(std::string(reinterpret_cast<const char*>(data), size));
	return 0;
}
