// Runs a fuzz target without libFuzzer, so that every compiler can build it and CTest can run it:
//
//     fuzz_<target> [file-or-directory ...] [--generated N] [--crash-dir DIR]
//
//   - every file given is replayed; a directory means every file in it, in name order (that is how corpora and
//     regression inputs are kept: tests/fuzz/corpus/<target>/);
//   - then N inputs of deterministic pseudo-random bytes (the same N inputs on every run and platform);
//   - a broken property aborts, as under libFuzzer. Before dying, the driver names the input that broke it; a
//     generated input is saved to DIR/crash-<n> (default: the working directory) with the command that replays it.
//
// With libFuzzer (SW_ENABLE_FUZZING) this file is not linked: libFuzzer provides main().

#include <algorithm>
#include <charconv>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <print>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
	#include <stdlib.h>
#endif

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace
{
	constexpr std::string_view Usage
			= "usage: fuzz_<target> [file-or-directory ...] [--generated N] [--crash-dir DIR]\n";
	constexpr unsigned GeneratorSeed = 20261005;
	constexpr std::size_t MaxGeneratedSize = 1024;

	// What is running now, for the crash handler. Set before each input; read only when the process is dying.
	struct Current
	{
		std::string program;
		std::string description;
		std::vector<std::uint8_t> bytes;
		std::optional<unsigned long> generatedIndex;
		std::filesystem::path crashDirectory{"."};
	};

	Current& current()
	{
		static Current state;
		return state;
	}

	// Not strictly async-signal-safe (it uses stdio), which is acceptable for a test driver that is about to die anyway:
	// the point is to say which input broke the target and keep a copy of it.
	extern "C" void onCrash(const int signal)
	{
		const Current& state = current();
		std::println(stderr, "\nfuzz: input that broke the target: {}", state.description);
		if (state.generatedIndex)
		{
			const std::filesystem::path file = state.crashDirectory / std::format("crash-{}", *state.generatedIndex);
			if (std::FILE* out = std::fopen(file.string().c_str(), "wb"))
			{
				std::fwrite(state.bytes.data(), 1, state.bytes.size(), out);
				std::fclose(out);
				std::println(
						stderr,
						"fuzz: saved to {}\nfuzz: replay it with: {} {}",
						file.string(),
						state.program,
						file.string());
			}
		}
		else
		{
			std::println(stderr, "fuzz: replay it with: {} {}", state.program, state.description);
		}
		std::fflush(stderr);
		std::signal(signal, SIG_DFL);
		std::raise(signal);
	}

	void run(std::string description, std::vector<std::uint8_t> bytes, const std::optional<unsigned long> index)
	{
		Current& state = current();
		state.description = std::move(description);
		state.bytes = std::move(bytes);
		state.generatedIndex = index;
		LLVMFuzzerTestOneInput(state.bytes.data(), state.bytes.size());
	}

	std::size_t replayPath(const std::filesystem::path& path)
	{
		if (std::filesystem::is_directory(path))
		{
			std::vector<std::filesystem::path> files;
			for (const auto& entry : std::filesystem::directory_iterator(path))
			{
				if (entry.is_regular_file())
				{
					files.push_back(entry.path());
				}
			}
			std::ranges::sort(files);
			for (const std::filesystem::path& file : files)
			{
				replayPath(file);
			}
			return files.size();
		}
		if (!std::filesystem::exists(path))
		{
			std::println(stderr, "fuzz: skipping {}: no such file or directory", path.string());
			return 0;
		}
		std::ifstream stream(path, std::ios::binary);
		std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		run(path.string(), std::move(bytes), std::nullopt);
		return 1;
	}

	void replayGenerated(const unsigned long count)
	{
		std::mt19937 engine(GeneratorSeed);
		for (unsigned long index = 0; index < count; ++index)
		{
			std::vector<std::uint8_t> bytes(engine() % (MaxGeneratedSize + 1));
			for (std::uint8_t& byte : bytes)
			{
				byte = static_cast<std::uint8_t>(engine() & 0xFFU);
			}
			run(std::format("generated input #{}", index), std::move(bytes), index);
		}
	}

	std::optional<unsigned long> parseCount(const std::string_view text)
	{
		unsigned long value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (error != std::errc{} || end != text.data() + text.size())
		{
			return std::nullopt;
		}
		return value;
	}
}

int main(int argc, char** argv)
{
#ifdef _WIN32
	// No "abort() has been called" dialog on Windows: just exit with the failure.
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
	const std::span<char*> arguments(argv, static_cast<std::size_t>(argc));
	current().program = arguments.empty() ? "fuzz_target" : arguments[0];

	unsigned long generated = 0;
	std::vector<std::filesystem::path> paths;
	for (std::size_t i = 1; i < arguments.size(); ++i)
	{
		const std::string_view argument = arguments[i];
		if (argument == "--generated" || argument == "--crash-dir")
		{
			if (i + 1 >= arguments.size())
			{
				std::print(stderr, "{}", Usage);
				return 2;
			}
			const std::string_view value = arguments[++i];
			if (argument == "--crash-dir")
			{
				current().crashDirectory = value;
				continue;
			}
			const std::optional<unsigned long> count = parseCount(value);
			if (!count)
			{
				std::print(stderr, "{}", Usage);
				return 2;
			}
			generated = *count;
			continue;
		}
		if (argument.starts_with("--"))
		{
			std::print(stderr, "{}", Usage);
			return 2;
		}
		paths.emplace_back(argument);
	}

	for (const int signal : {SIGABRT, SIGSEGV, SIGFPE, SIGILL})
	{
		std::signal(signal, onCrash);
	}

	std::size_t files = 0;
	for (const std::filesystem::path& path : paths)
	{
		files += replayPath(path);
	}
	replayGenerated(generated);
	std::println("fuzz: replayed {} corpus files and {} generated inputs", files, generated);
	return 0;
}
