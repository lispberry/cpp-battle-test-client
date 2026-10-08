---
description: Review C++ changes for modern C++23 style and this repo's conventions (strong types, std::expected, std::print)
argument-hint: "[paths | branch | 'all'] (default: this branch vs main, plus uncommitted changes)"
allowed-tools: Bash(git:*), Bash(uv:*), Bash(cmake:*), Bash(ctest:*), Read, Grep, Glob
---

Review C++ code in this repository and report findings. **Do not edit anything** unless the user asks
for fixes after the review.

Scope from the user (may be empty): $ARGUMENTS

## 1. Collect the code under review

- Empty scope: everything this branch changes, committed or not:
  ```bash
  git diff main...HEAD --stat; git diff HEAD --stat; git status --short
  git diff main...HEAD; git diff HEAD
  ```
- Paths: those files. A branch name: `git diff main...<branch>`. `all`: every file in `src/`.

Read each changed file **in full**, not only the hunks: most of the rules below concern how new code
fits the code around it.

## 2. Learn the house style first

Read these before judging anything; the review must match them, not a generic style guide:

- `src/Core/Base/StrongType.hpp` and `src/Core/Model/Types.hpp`: the strong types and the few
  operators deliberately defined on them.
- `src/Core/Model/Errors.hpp`, `src/Core/IO/ScenarioError.hpp`: the error enums used with `std::expected`.
- `README.md`, sections *Как устроен юнит* and *Тесты и проверки*.

## 3. What to look for

### Strong types (`StrongType<T, Tag>`)
- A raw `uint32_t`/`int`/`std::string_view` carrying a domain quantity (id, hp, damage, chance,
  distance, speed, round, ability, unit name) where a `StrongType` alias exists: use the alias.
- A new domain quantity passed around as a raw number: propose a new alias in `Types.hpp`
  (`using Armor = StrongType<uint32_t, struct ArmorTag>;`).
- Arithmetic is not generic on purpose. A needed operation becomes a named free operator or function
  next to the alias, with its rule (`Hp operator-(Hp, Damage)` saturates at zero). Never propose adding
  operators to `StrongType` itself.
- `.get()` belongs at boundaries only: building log records, parsing scenario text, indexing storage.
  `.get()` in game logic usually means a missing operator or a wrong type.
- Two parameters of the same raw type next to each other (`attacker`, `target`) are a swap bug waiting
  to happen; strong types fix that.

### Errors: `std::expected` with explicit error types
- A failure the caller must handle returns `std::expected<T, E>`, where `E` is an `enum class` with an
  explicit underlying type (`Idle`, `WorldError`) or a small struct of one (`ScenarioError`). Not
  `bool`, not `std::optional` when the reason matters, not sentinel values (`-1`, `0`, empty string),
  not out-parameters, and never `std::expected<T, std::string>`: a message is made from the code with
  `describe(...)` where it is shown.
- A new failure reason is a new enumerator, with its `describe` text.
- Exceptions only for broken invariants and programmer errors (`std::invalid_argument`,
  `std::logic_error`, `std::out_of_range`), at construction and API boundaries, as the existing code does.
  An exception used for control flow, or `std::expected` used for a bug that can never be handled, is
  a finding.
- Chain with the monadic members (`and_then`, `transform`, `or_else`, `transform_error`) when it makes
  the code shorter. A ladder of `if (!x) return std::unexpected(x.error());` is acceptable only when
  each step needs its own name.
- Every function returning `std::expected` or `std::optional` is `[[nodiscard]]`.

### Output: `std::print` over iostream
- New `std::cout <<`, `std::cerr <<`, `printf` or `std::stringstream` formatting: use
  `std::print`/`std::println` (`<print>`) and `std::format`. To write to a stream the code is handed,
  use `std::print(stream, ...)`, which takes a `std::ostream&` in C++23.
- Existing iostream code (`src/main.cpp`, `TextLogPrinter`) is not a finding unless the change touches
  it; then suggest the switch in the same change.
- `std::format`/`std::print` arguments of a `StrongType` go through `.get()`; propose a `std::formatter`
  specialisation only if the same `.get()` is repeated in many format calls.

### Modern C++23, in this repo's style
- **Concepts on every template parameter**, including bare `auto` in lambdas (`sw-constrained-templates`):
  `template <Component C>`, `[]<ClassType A>(A& action)`. Prefer a named concept from
  `Core/Base/Concepts.hpp` or the module over an ad-hoc `requires` clause.
- **Ranges.** `std::ranges::` algorithms with projections (`std::ranges::find(xs, id, &Logged::target)`)
  over iterator pairs and hand-written search loops; views and pipes for transformations;
  `std::ranges::to<std::vector>()` to materialise. A plain `for` loop that reads better stays.
- **`auto` for call results** (`const auto target = ...`, `auto*` for pointers); spelled types for
  literals and designated initialisers.
- **Designated initialisers** for aggregates (`UnitMoved{.unitId = ..., .x = ..., .y = ...}`).
- **Non-owning parameters:** `std::span`, `std::string_view`, `const T&`. Owning members only:
  `std::unique_ptr`, values, `std::optional` (see *Ownership*); raw owning pointers or `new`/`delete`
  are findings.
- **`constexpr`** for anything computable at compile time; `static constexpr` for per-type names
  (`static constexpr Ability Name{"rending"}`); `if constexpr` over tag dispatch and SFINAE.
- **`std::variant` + `std::visit`** with a constrained generic lambda or overload set over class
  hierarchies for closed sets of alternatives.
- **`enum class` with an explicit underlying type**; `std::to_underlying` over `static_cast` to the
  underlying type.
- **`std::unreachable()`** after a `switch` that covers every enumerator, not a `default:` that hides a
  missing case from `-Wswitch`.
- **`[[nodiscard]]`** on functions whose result is the point of calling them.
- **Template code and coverage:** every file in `src/` needs 100% line *and branch* coverage, counted per
  template instantiation. Branches that multiply with instantiations (a `||` fold over a kit, a check
  inside a lambda passed per call site) get a design suggestion: move the branch into a template on
  fewer parameters (see `detail::tryAct` in `Core/Units/Unit.hpp`, `detail::ChangeOf` in
  `Core/Events/Action.hpp`).
- Suggest a newer feature only where it makes this code simpler, not to show it off. Check the
  standard library in use (libc++ and libstdc++) supports it before proposing it.

### Repo conventions
- Includes spelled from `src/` (`<Core/World/World.hpp>`), short form only inside the own feature;
  module layering (`check.py layers`); features never include each other.
- Program against interfaces (`Map`, `RandomSource`, `LogSink`); concrete types only where the program is
  assembled and in tests.
- One word per idea (*report* a `Record`, a `LogSink` *writes* it; `randomSource()`); commands end in
  `Command`; an ability is its component's `Name`.
- Comments: `///` on public API in `src/Core/` (concepts and class templates with a `\code` example). In
  `src/Features/`, a comment only when it states a rule the code does not show; a comment that restates
  a name, a signature or a kit is a finding.
- Markdown only in `README.md`, `docs/` and `.claude/commands/`.
- New behaviour needs tests in `tests/unit/` mirroring `src/`, using the fakes in `tests/unit/Support/`.

### Correctness
Style does not replace a bug hunt: also report lifetime and dangling-reference problems (views over
temporaries, `std::string_view` of a temporary), unchecked `std::expected`/`optional` access,
overflow and saturation in arithmetic on hp and damage, and iteration over containers that the loop
changes.

## 4. Run the cheap checks

```bash
uv run tools/scripts/check.py layers
uv run tools/scripts/check.py markdown
uv run tools/scripts/check.py tidy        # staged changes; `--all` when the scope is `all`
```

Report their failures as findings. Do not repeat what clang-format would fix.

## 5. Report

Group the findings by severity: **Bugs**, then **Design** (error handling, types, ownership), then
**Style** (modernisation, conventions). For each one:

- `path/to/file.cpp:line`: what is wrong, in one sentence;
- why it matters here (the rule above it breaks, or the failure it can cause);
- the fix, as a short code snippet when it is not obvious.

Leave out anything you are not confident about rather than padding the list. If the code is clean, say
so in one line. End with the 1–3 changes you would make first.
