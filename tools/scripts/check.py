#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///
"""Code checks for sw_battle_test: formatting and clang-tidy (including the include rules, sw-include-style).

    uv run tools/scripts/check.py format              changed lines of staged files (pre-commit)
    uv run tools/scripts/check.py format --all        every C++ file in src/
    uv run tools/scripts/check.py format --fix        reformat the changed lines of staged files
    uv run tools/scripts/check.py tidy                changed lines of staged files (pre-commit)
    uv run tools/scripts/check.py tidy --all          every translation unit in src/
    uv run tools/scripts/check.py tidy --fix          apply clang-tidy's fixes (combine with --all for the whole tree)
    uv run tools/scripts/check.py tidy --self-test    the project's own checks (sw-*) against their fixtures
    uv run tools/scripts/check.py layers              every #include respects the module layering (Core never includes
                                                      Features, the simulation never includes IO, ...)
    uv run tools/scripts/check.py markdown            Markdown only in README.md, docs/ and .claude/
    uv run tools/scripts/check.py fuzz --list         list the libFuzzer targets in tests/fuzz/Targets/
    uv run tools/scripts/check.py fuzz <target>       build it with LLVM's Clang + libFuzzer and fuzz for 60 s
                                                      (--time S, --jobs N; arguments after -- go to libFuzzer)
    uv run tools/scripts/check.py coverage            build with the coverage preset, run all tests, require 100% line
                                                      and branch coverage of every file in src/

Standard library only, so plain `python3 tools/scripts/check.py ...` works as well.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from functools import lru_cache
from pathlib import Path
from typing import Optional, Sequence

REPO_ROOT = Path(__file__).resolve().parents[2]  # tools/scripts/check.py
SRC_ROOT = REPO_ROOT / "src"
BUILD_DIR = Path(os.environ.get("SW_LINT_BUILD_DIR", REPO_ROOT / "build" / "debug"))

# Keep in sync with the LLVM version used in CI, so local and CI results match.
LLVM_MAJOR = 23
CPP_EXTENSIONS = ("cpp", "hpp", "h", "cc", "cxx", "hxx")


def fail(message: str) -> None:
	print(f"error: {message}", file=sys.stderr)
	sys.exit(1)


# --------------------------------------------------------------------------- LLVM tools


def llvm_bin_dirs() -> list[Path]:
	"""Places LLVM lives when it is not on PATH (Homebrew's llvm is keg-only, apt's is versioned)."""
	dirs: list[Path] = []
	brew = shutil.which("brew")
	if brew:
		prefix = subprocess.run([brew, "--prefix", "llvm"], capture_output=True, text=True, check=False).stdout.strip()
		if prefix:
			dirs.append(Path(prefix) / "bin")
	dirs += [
		Path("/opt/homebrew/opt/llvm/bin"),
		Path("/usr/local/opt/llvm/bin"),
		Path(f"/usr/lib/llvm-{LLVM_MAJOR}/bin"),
		Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM" / "bin",
	]
	return dirs


@lru_cache(maxsize=None)
def find_tool(name: str) -> Path:
	for candidate in (name, f"{name}-{LLVM_MAJOR}"):
		found = shutil.which(candidate)
		if found:
			return Path(found)
	for directory in llvm_bin_dirs():
		for candidate in (name, f"{name}.py"):
			if (directory / candidate).is_file():
				return directory / candidate
	fail(f"'{name}' not found. Install LLVM {LLVM_MAJOR} with tools/scripts/mac/setup.sh, tools/scripts/linux/setup.sh or "
		 "tools/scripts/win/setup.ps1")
	raise AssertionError  # unreachable


def python_tool(path: Path) -> list[str]:
	"""LLVM's helper scripts are Python; run them with this interpreter so it works on Windows too."""
	return [sys.executable, str(path)]


# --------------------------------------------------------------------------- git


def git(*args: str) -> str:
	return subprocess.run(["git", *args], cwd=REPO_ROOT, capture_output=True, text=True, check=True).stdout


def staged_cpp_files() -> list[str]:
	patterns = [f"*.{extension}" for extension in CPP_EXTENSIONS]
	return git("diff", "--cached", "--name-only", "--diff-filter=ACMR", "--", *patterns).split()


def all_cpp_files() -> list[Path]:
	return sorted(path for path in SRC_ROOT.rglob("*") if path.suffix.lstrip(".") in CPP_EXTENSIONS)


def cmake_cache_value(name: str) -> Optional[str]:
	cache = BUILD_DIR / "CMakeCache.txt"
	if not cache.is_file():
		return None
	for line in cache.read_text(encoding="utf-8").splitlines():
		if line.startswith(f"{name}:"):
			return line.split("=", 1)[1]
	return None


def ensure_build_dir(clang_tidy: Path) -> None:
	"""Configures BUILD_DIR for compile_commands.json, with the tidy plugin built for this clang-tidy's LLVM."""
	configured = (BUILD_DIR / "compile_commands.json").is_file()
	if configured and cmake_cache_value("SW_CLANG_TIDY") == str(clang_tidy):
		return
	print(f"clang-tidy: configuring {BUILD_DIR}")
	command = ["cmake", "-S", str(REPO_ROOT), "-B", str(BUILD_DIR), f"-DSW_CLANG_TIDY={clang_tidy}"]
	if not configured:
		command += ["-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
	subprocess.run(command, check=True, stdout=subprocess.DEVNULL)


# --------------------------------------------------------------------------- format


def cmd_format(args: argparse.Namespace) -> int:
	clang_format = find_tool("clang-format")

	if args.all:
		files = [str(path) for path in all_cpp_files()]
		return subprocess.run([str(clang_format), "--dry-run", "--Werror", *files], cwd=REPO_ROOT, check=False).returncode

	git_clang_format = [*python_tool(find_tool("git-clang-format")), "--binary", str(clang_format)]
	options = ["--staged", "--extensions", ",".join(CPP_EXTENSIONS)]

	if args.fix:
		subprocess.run([*git_clang_format, *options], cwd=REPO_ROOT, check=False)
		print("Reformatted the working tree. Review and `git add` the changes.")
		return 0

	result = subprocess.run([*git_clang_format, *options, "--diff"], cwd=REPO_ROOT, capture_output=True, text=True,
			check=False)
	output = result.stdout.strip()
	if not output or "no modified files to format" in output or "did not modify any files" in output:
		return 0

	print(output)
	print("\nerror: staged changes are not clang-format clean. Fix with:\n  uv run tools/scripts/check.py format --fix")
	return 1


# --------------------------------------------------------------------------- tidy


TIDY_PLUGIN_TARGET = "sw_tidy_checks"
TIDY_PLUGIN = BUILD_DIR / "SwTidyChecks.so"  # fixed name, see tools/clang-tidy-checks/CMakeLists.txt


def tidy_plugin_args() -> list[str]:
	"""Builds the project's own clang-tidy checks (sw-*) and returns the arguments that load them."""
	if os.name == "nt":
		print("warning: clang-tidy cannot load plugins on Windows; project checks (sw-*) are skipped", file=sys.stderr)
		return []
	result = subprocess.run(["cmake", "--build", str(BUILD_DIR), "--target", TIDY_PLUGIN_TARGET], cwd=REPO_ROOT,
			capture_output=True, text=True, check=False)
	if result.returncode != 0 or not TIDY_PLUGIN.is_file():
		print(result.stdout + result.stderr, file=sys.stderr)
		fail(f"could not build the clang-tidy plugin ({TIDY_PLUGIN_TARGET}). It needs LLVM {LLVM_MAJOR} with its CMake "
			 f"package and headers; `cmake -S . -B {BUILD_DIR}` says why it was skipped")
	return ["-load", str(TIDY_PLUGIN)]


def drop_compiler_fixes(fixes: str) -> str:
	"""Removes the compiler's own diagnostics (clang-diagnostic-*) from an --export-fixes file.

	Their fix-its can contradict the project's checks: for `#include <Unit.hpp>` that only resolves next to the
	including file, clang suggests quotes while sw-include-style suggests the full path. Both edit the same range, so
	clang-tidy would apply neither. Dropping the compiler's lets the project's fix win.
	"""
	kept: list[str] = []
	skipping = False
	for line in fixes.splitlines(keepends=True):
		if line.startswith("  - DiagnosticName:"):
			skipping = line.split(":", 1)[1].strip().strip("'").startswith("clang-diagnostic-")
		elif not line.startswith("    "):
			skipping = False
		if not skipping:
			kept.append(line)
	return "".join(kept)


def apply_fixes(fixes_dir: Path) -> None:
	for fixes in fixes_dir.glob("*.yaml"):
		fixes.write_text(drop_compiler_fixes(fixes.read_text(encoding="utf-8")), encoding="utf-8")
	subprocess.run([str(find_tool("clang-apply-replacements")), str(fixes_dir)], cwd=REPO_ROOT, check=True)
	print("\nApplied clang-tidy's fixes to the working tree. Review and `git add` them, then rerun the check.")


TIDY_FIXTURES = REPO_ROOT / "tools" / "clang-tidy-checks" / "test"


def tidy_self_test(clang_tidy: Path, plugin: list[str]) -> int:
	"""Runs each project check on its fixture: exactly the lines marked `// EXPECT` must be reported."""
	sysroot: list[str] = []
	if sys.platform == "darwin" and shutil.which("xcrun"):
		sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False).stdout.strip()
		sysroot = ["-isysroot", sdk] if sdk else []
	failures = 0
	for fixture in sorted(TIDY_FIXTURES.rglob("*.cpp")):
		check = "sw-" + fixture.stem.replace("_", "-")
		output = subprocess.run([str(clang_tidy), *plugin, f"-checks=-*,{check}", str(fixture), "--", "-std=c++23",
				*sysroot], capture_output=True, text=True, check=False).stdout
		reported = {int(match) for match in re.findall(rf"{re.escape(fixture.name)}:(\d+):\d+: (?:warning|error):", output)}
		expected = {number for number, line in enumerate(fixture.read_text(encoding="utf-8").splitlines(), 1)
				if "// EXPECT" in line}
		if reported != expected:
			failures += 1
			print(f"{check}: expected reports on lines {sorted(expected)}, got {sorted(reported)}\n{output}",
					file=sys.stderr)
		else:
			print(f"{check}: ok ({len(expected)} expected reports)")
	return 1 if failures else 0


def tidy_unit(staged: Path) -> Path:
	"""The translation unit clang-tidy checks for a staged file: the file itself, or a header's header-check source."""
	relative = staged.relative_to("src")
	if staged.suffix in (".hpp", ".h", ".hxx"):
		return BUILD_DIR / "header-check" / f"{relative.as_posix()}.cpp"
	return REPO_ROOT / staged


def cmd_tidy(args: argparse.Namespace) -> int:
	clang_tidy = find_tool("clang-tidy")
	ensure_build_dir(clang_tidy)
	plugin = tidy_plugin_args()
	if args.self_test:
		return tidy_self_test(clang_tidy, plugin)
	jobs = str(os.cpu_count() or 4)

	with tempfile.TemporaryDirectory(prefix="sw-tidy-fixes-") as fixes_dir:
		# One YAML file per translation unit; a directory needs no PyYAML to merge them.
		export = ["-export-fixes", f"{fixes_dir}{os.sep}"] if args.fix else []

		if args.all:
			# Only src/ (the build also compiles the plugin itself, which is not held to the project's checks), plus
			# the header-check sources: one per header, so that headers no .cpp includes are checked too.
			sources = f"^({re.escape(str(SRC_ROOT))}|{re.escape(str(BUILD_DIR / 'header-check'))})/"
		else:
			# The staged files, checked exactly as `--all` checks them: a header through its header-check source (a
			# header given to clang-tidy as the main file gets guessed flags and different misc-include-cleaner
			# results), so that the hook and `--all` never disagree.
			units = [tidy_unit(Path(staged)) for staged in staged_cpp_files() if staged.startswith("src/")]
			if not units:
				return 0
			sources = "^(" + "|".join(re.escape(str(unit)) for unit in units) + ")$"
		run_clang_tidy = python_tool(find_tool("run-clang-tidy"))
		returncode = subprocess.run(
				[*run_clang_tidy, "-p", str(BUILD_DIR), "-clang-tidy-binary", str(clang_tidy), *plugin, *export,
				 "-j", jobs, "-quiet", sources],
				cwd=REPO_ROOT,
				check=False,
		).returncode

		if args.fix:
			apply_fixes(Path(fixes_dir))
			return 0
		return returncode


# --------------------------------------------------------------------------- layers


# Which modules each module may include (besides itself and the standard library). The engine is layered bottom-up;
# features build on the whole engine and on Features/Common, never on each other; App wires everything together.
CORE_LAYERS: dict[str, set[str]] = {
	"Core/Base": set(),
	"Core/Model": {"Core/Base"},
	"Core/Events": {"Core/Base", "Core/Model", "Core/Log"},
	"Core/Units": {"Core/Base", "Core/Model", "Core/Events"},
	"Core/World": {"Core/Base", "Core/Model", "Core/Events", "Core/Units"},
	"Core/Query": {"Core/Base", "Core/Model", "Core/Events", "Core/Units", "Core/World"},
	"Core/Log": {"Core/Base", "Core/Model"},
	"Core/Simulation": {"Core/Base", "Core/Model", "Core/Events", "Core/Units", "Core/World", "Core/Query", "Core/Log"},
	"Core/IO": {"Core/Base", "Core/Model", "Core/Events", "Core/Units", "Core/World", "Core/Query", "Core/Log",
			"Core/Simulation"},
}
ALL_CORE = set(CORE_LAYERS)


def module_of(relative: str) -> Optional[str]:
	"""`Core/Query/Units.hpp` -> `Core/Query`, `Features/Hunter/Hunter.hpp` -> `Features/Hunter`, `App/x` -> `App`."""
	parts = relative.split("/")
	if parts[0] == "App" or len(parts) == 1:
		return parts[0] if parts[0] == "App" else None
	if len(parts) == 2:  # Features/Features.cpp
		return parts[0]
	return f"{parts[0]}/{parts[1]}"


def allowed_modules(module: str) -> Optional[set[str]]:
	if module in CORE_LAYERS:
		return CORE_LAYERS[module] | {module}
	if module == "Features":  # the registration list: every feature
		return None
	if module.startswith("Features/"):
		return ALL_CORE | {"Features/Common", module}
	return None  # App and main.cpp may include anything


INCLUDE_PATTERN = re.compile(r"^\s*#\s*include\s*<((?:Core|Features|App)/[^>]+)>", re.MULTILINE)


def cmd_layers(_args: argparse.Namespace) -> int:
	violations = []
	for path in all_cpp_files():
		relative = path.relative_to(SRC_ROOT).as_posix()
		module = module_of(relative)
		allowed = allowed_modules(module) if module else None
		if allowed is None:
			continue
		for include in INCLUDE_PATTERN.findall(path.read_text(encoding="utf-8")):
			target = module_of(include)
			if target not in allowed:
				violations.append(f"src/{relative}: {module} must not include <{include}> ({target})")
	for violation in violations:
		print(violation, file=sys.stderr)
	if violations:
		print("\nerror: module layering broken; see CORE_LAYERS in tools/scripts/check.py",
				file=sys.stderr)
		return 1
	return 0


# --------------------------------------------------------------------------- markdown


def cmd_markdown(_args: argparse.Namespace) -> int:
	files = subprocess.run(["git", "ls-files", "--cached", "--others", "--exclude-standard", "*.md"], cwd=REPO_ROOT,
			capture_output=True, text=True, check=True).stdout.splitlines()
	# Claude Code reads its commands from .claude/, so they are the one exception besides the root README.
	misplaced = [path for path in files if path != "README.md" and not path.startswith(("docs/", ".claude/"))]
	for path in misplaced:
		print(f"{path}: Markdown belongs in docs/", file=sys.stderr)
	if misplaced:
		print("\nerror: only README.md, docs/ and .claude/ may hold Markdown", file=sys.stderr)
		return 1
	return 0


# --------------------------------------------------------------------------- coverage


COVERAGE_PRESET = "coverage"
COVERAGE_DIR = REPO_ROOT / "build" / COVERAGE_PRESET
# The process boundary: its catch blocks handle failures no scenario can provoke. Its happy path is run by the e2e
# tests; everything it calls lives in src/App/ and is held to the 100% rule.
COVERAGE_EXEMPT = {"main.cpp"}


@lru_cache(maxsize=None)
def compiler_is_apple_clang() -> bool:
	compiler = cmake_cache_value_in(COVERAGE_DIR, "CMAKE_CXX_COMPILER")
	if not compiler:
		return False
	version = subprocess.run([compiler, "--version"], capture_output=True, text=True, check=False).stdout
	return version.startswith("Apple clang")


def coverage_tool(name: str) -> list[str]:
	"""llvm-profdata / llvm-cov matching the compiler: Xcode's for AppleClang (the profile format is versioned)."""
	if compiler_is_apple_clang() and shutil.which("xcrun"):
		return ["xcrun", name]
	return [str(find_tool(name))]


def cmake_cache_value_in(build_dir: Path, name: str) -> Optional[str]:
	cache = build_dir / "CMakeCache.txt"
	if not cache.is_file():
		return None
	for line in cache.read_text(encoding="utf-8").splitlines():
		if line.startswith(f"{name}:"):
			return line.split("=", 1)[1]
	return None


def cmd_coverage(args: argparse.Namespace) -> int:
	import json

	subprocess.run(["cmake", "--preset", COVERAGE_PRESET], cwd=REPO_ROOT, check=True, stdout=subprocess.DEVNULL)
	subprocess.run(["cmake", "--build", "--preset", COVERAGE_PRESET], cwd=REPO_ROOT, check=True)

	profiles = COVERAGE_DIR / "profiles"
	shutil.rmtree(profiles, ignore_errors=True)
	profiles.mkdir(parents=True)
	environment = dict(os.environ, LLVM_PROFILE_FILE=str(profiles / "%p-%m.profraw"))
	tests = subprocess.run(["ctest", "--preset", COVERAGE_PRESET, "-j", str(os.cpu_count() or 4)], cwd=REPO_ROOT,
			env=environment, check=False)
	if tests.returncode != 0:
		fail("tests failed; coverage is only measured on a passing run")

	merged = COVERAGE_DIR / "coverage.profdata"
	subprocess.run([*coverage_tool("llvm-profdata"), "merge", "-sparse", *map(str, profiles.glob("*.profraw")), "-o",
			str(merged)], check=True)

	binaries = [COVERAGE_DIR / "Debug" / "sw_battle_test", COVERAGE_DIR / "tests" / "unit" / "sw_unit_tests"]
	objects = [str(binaries[0])] + [arg for binary in binaries[1:] for arg in ("-object", str(binary))]
	export = subprocess.run([*coverage_tool("llvm-cov"), "export", "-summary-only", f"-instr-profile={merged}", *objects],
			capture_output=True, text=True, check=True)
	files = json.loads(export.stdout)["data"][0]["files"]

	rows = []
	failed = False
	for entry in sorted(files, key=lambda item: item["filename"]):
		path = Path(entry["filename"])
		try:
			relative = path.relative_to(SRC_ROOT)
		except ValueError:
			continue
		summary = entry["summary"]
		lines, branches = summary["lines"], summary["branches"]
		line_percent = 100.0 if lines["count"] == 0 else 100.0 * lines["covered"] / lines["count"]
		branch_percent = 100.0 if branches["count"] == 0 else 100.0 * branches["covered"] / branches["count"]
		exempt = str(relative) in COVERAGE_EXEMPT
		complete = line_percent == 100.0 and branch_percent == 100.0
		failed |= not complete and not exempt
		mark = "exempt" if exempt else ("ok" if complete else "MISSING")
		rows.append((str(relative), line_percent, branch_percent, mark))

	module_width = max(len(name) for name, *_ in rows)
	print(f"\n{'file':<{module_width}}  {'lines':>7}  {'branches':>8}")
	for name, line_percent, branch_percent, mark in rows:
		if args.verbose or mark != "ok":
			print(f"{name:<{module_width}}  {line_percent:6.1f}%  {branch_percent:7.1f}%  {mark}")
	print(f"{len(rows)} files in src/, {sum(1 for row in rows if row[3] == 'ok')} fully covered")
	if failed:
		for name, _, _, mark in rows:
			if mark == "MISSING":
				print_uncovered(SRC_ROOT / name, merged, objects)
		print("\nerror: every file in src/ needs 100% line and branch coverage. Details, e.g.:\n"
			  f"  {' '.join(coverage_tool('llvm-cov'))} show -instr-profile={merged} {' '.join(objects)} "
			  "-show-branches=count <file>", file=sys.stderr)
		return 1
	return 0


def print_uncovered(path: Path, profile: Path, objects: list[str]) -> None:
	"""The lines of `path` that never ran, then its branches taken only one way, as llvm-cov shows them. A line inside a
	template is listed per instantiation (prefixed with `|`)."""
	shown = subprocess.run([*coverage_tool("llvm-cov"), "show", f"-instr-profile={profile}", *objects,
			"-show-branches=count", "-show-instantiations", str(path)], capture_output=True, text=True,
			check=False).stdout
	lines = sorted({line.strip() for line in shown.splitlines() if re.match(r"^[\s|]*\d+\|\s+0\|", line)})
	branches = sorted({line.strip() for line in shown.splitlines() if re.search(r"(True|False): 0[],]", line)})
	print(f"\n{path.relative_to(REPO_ROOT)}: {len(lines)} lines never ran, {len(branches)} branches one way", file=sys.stderr)
	for line in lines[:30] + branches[:10]:
		print(f"  {line}", file=sys.stderr)


# --------------------------------------------------------------------------- fuzz


FUZZ_DIR = REPO_ROOT / "build" / "fuzz"
FUZZ_TARGETS = REPO_ROOT / "tests" / "fuzz" / "Targets"
FUZZ_CORPORA = REPO_ROOT / "tests" / "fuzz" / "corpus"


def llvm_clang() -> Path:
	"""LLVM's clang++: AppleClang has no libFuzzer runtime."""
	for directory in llvm_bin_dirs():
		candidate = directory / "clang++"
		if candidate.is_file():
			return candidate
	found = shutil.which(f"clang++-{LLVM_MAJOR}") or shutil.which("clang++")
	if found and not subprocess.run([found, "--version"], capture_output=True, text=True,
			check=False).stdout.startswith("Apple clang"):
		return Path(found)
	fail(f"LLVM's clang++ not found (AppleClang ships no libFuzzer). Install LLVM {LLVM_MAJOR}: tools/scripts/<os>/setup")
	raise AssertionError  # unreachable


def cmd_fuzz(args: argparse.Namespace) -> int:
	targets = sorted(path.stem for path in FUZZ_TARGETS.glob("*.cpp"))
	if args.list or not args.target:
		print("\n".join(targets))
		return 0
	if args.target not in targets:
		fail(f"no fuzz target '{args.target}'; one of: {', '.join(targets)}")

	clang = llvm_clang()
	subprocess.run(["cmake", "-S", str(REPO_ROOT), "-B", str(FUZZ_DIR), "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
			f"-DCMAKE_CXX_COMPILER={clang}", "-DSW_ENABLE_FUZZING=ON", "-DSW_BUILD_TIDY_PLUGIN=OFF"],
			cwd=REPO_ROOT, check=True, stdout=subprocess.DEVNULL)
	binary_target = f"fuzz_{args.target}"
	subprocess.run(["cmake", "--build", str(FUZZ_DIR), "--target", binary_target], cwd=REPO_ROOT, check=True)
	binary = FUZZ_DIR / "tests" / "fuzz" / binary_target

	# libFuzzer adds what it finds to the first directory; the repository corpus (regressions) is only read.
	working = FUZZ_DIR / "corpus" / args.target
	working.mkdir(parents=True, exist_ok=True)
	seeds = FUZZ_CORPORA / args.target
	artifacts = FUZZ_DIR / "artifacts" / args.target
	artifacts.mkdir(parents=True, exist_ok=True)
	command = [str(binary), str(working), *([str(seeds)] if seeds.is_dir() else []),
			f"-artifact_prefix={artifacts}{os.sep}", f"-max_total_time={args.time}", *args.libfuzzer]
	if args.jobs > 1:
		command += [f"-jobs={args.jobs}", f"-workers={args.jobs}"]
	print(" ".join(command))
	result = subprocess.run(command, cwd=FUZZ_DIR, check=False)
	if result.returncode != 0:
		crashes = sorted(artifacts.glob("*"), key=lambda path: path.stat().st_mtime)
		if crashes:
			crash = crashes[-1]
			print(f"\nA crash was saved to {crash}. Keep it as a regression test (CTest replays it everywhere):\n"
				  f"  mkdir -p {seeds} && cp {crash} {seeds}/\n"
				  f"Replay it in a normal build:\n  build/debug/tests/fuzz/{binary_target} {crash}", file=sys.stderr)
	return result.returncode


# --------------------------------------------------------------------------- main


def main(argv: Sequence[str]) -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	commands = parser.add_subparsers(dest="command", required=True)

	format_parser = commands.add_parser("format", help="clang-format check")
	format_mode = format_parser.add_mutually_exclusive_group()
	format_mode.add_argument("--all", action="store_true", help="check every file in src/, not just staged changes")
	format_mode.add_argument("--fix", action="store_true", help="reformat the changed lines of staged files")
	format_parser.set_defaults(handler=cmd_format)

	tidy_parser = commands.add_parser("tidy", help="clang-tidy check")
	tidy_parser.add_argument("--all", action="store_true", help="check every translation unit in src/")
	tidy_parser.add_argument("--fix", action="store_true", help="apply the suggested fixes to the working tree")
	tidy_parser.add_argument("--self-test", action="store_true",
			help="run the project's own checks on tools/clang-tidy-checks/test/ fixtures")
	tidy_parser.set_defaults(handler=cmd_tidy)

	layers_parser = commands.add_parser("layers", help="module layering of #include directives")
	layers_parser.set_defaults(handler=cmd_layers)

	markdown_parser = commands.add_parser("markdown", help="Markdown only in README.md, docs/ and .claude/")
	markdown_parser.set_defaults(handler=cmd_markdown)

	fuzz_parser = commands.add_parser("fuzz", help="libFuzzer: build and run a fuzz target")
	fuzz_parser.add_argument("target", nargs="?", help="a target in tests/fuzz/Targets/ (without .cpp)")
	fuzz_parser.add_argument("--list", action="store_true", help="list the targets")
	fuzz_parser.add_argument("--time", type=int, default=60, help="seconds to fuzz (default 60)")
	fuzz_parser.add_argument("--jobs", type=int, default=1, help="parallel fuzzing processes")
	fuzz_parser.set_defaults(handler=cmd_fuzz)

	coverage_parser = commands.add_parser("coverage", help="100%% line and branch coverage of src/")
	coverage_parser.add_argument("--verbose", action="store_true", help="list fully covered files too")
	coverage_parser.set_defaults(handler=cmd_coverage)

	# Everything after `--` goes to the tool being driven (libFuzzer for `fuzz`), untouched by argparse.
	passthrough: list[str] = []
	if "--" in argv:
		split = list(argv).index("--")
		argv, passthrough = list(argv)[:split], list(argv)[split + 1:]
	args = parser.parse_args(argv)
	args.libfuzzer = passthrough
	return args.handler(args)


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))
