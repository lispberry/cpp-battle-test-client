---
description: Stage all current changes and commit them, fixing any pre-commit hook failures
argument-hint: "[optional extra context or scope for the message]"
allowed-tools: Bash(git:*), Bash(lefthook:*), Bash(uv:*), Bash(tools/*), Bash(cmake:*), Bash(brew:*), Read, Edit, Write, Grep, Glob
---

Stage everything that changed in the working tree and create one commit. If the
pre-commit hooks reject the commit, fix the underlying problem and commit again.

Extra context from the user (may be empty): $ARGUMENTS

## 1. Inspect the working tree

Run these together and read the output before doing anything else:

```bash
git status --porcelain=v1 -uall
git diff --stat HEAD 2>/dev/null || git diff --stat
git diff HEAD 2>/dev/null || git diff
git log --oneline -10 2>/dev/null
```

- If there is nothing to commit, say so and stop. Do not create an empty commit.
- Read the full diff, not just the stat. The commit body has to describe every
  change, so you need to have actually seen them.
- If the diff is huge, read it in chunks (`git diff HEAD -- <path>`) until you
  have covered all of it.

## 2. Stage

```bash
git add -A
```

`.gitignore` already excludes build output and `.DS_Store`. If `git status`
shows something that clearly should not be tracked (build artifacts, local
scratch files, secrets, large binaries), do not stage it — add it to
`.gitignore` instead and mention that in the commit body.

Never stage with `git commit -a`; always stage explicitly so the hooks see the
same content you reviewed.

## 3. Write the message

Format — a one-line summary, a blank line, then the detailed body:

```
<summary: imperative, <= 72 chars, no trailing period>

<body>
```

Summary line:
- One line, imperative mood ("add", "fix", "rename" — not "added"/"adds").
- Describes the change as a whole, not the biggest file in it.
- No `Conventional Commits` prefix unless `git log` shows the repo already uses one.

Body:
- Cover **all** the changes, not just the headline one. Group related edits
  under short `- ` bullets; use `path/to/file.cpp:` prefixes or small
  sub-headings when the change spans several unrelated areas.
- For each group say *what* changed and *why* — the reason is the part the diff
  cannot show. Include behaviour changes, new invariants, removed code, renames,
  build/CMake/tooling/config changes, and anything a reviewer would otherwise
  have to reverse-engineer.
- Mention follow-up work or known gaps if there are any.
- Wrap at ~72 columns.
- Do not describe the commit process itself, and do not mention hook fixes you
  made while producing this commit unless they changed real source behaviour.
- Match the language of the existing history (`git log`); use English if the
  history is empty.

End the commit message with exactly:

```
Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

Pass the message via a heredoc so formatting survives:

```bash
git commit -F - <<'EOF'
<summary>

<body>

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
EOF
```

## 4. Handle pre-commit hook failures

This repo uses lefthook (`lefthook.yml`) with two jobs that check **only the
changed lines** of staged C++ files:

| Job | Script | Checks |
| --- | --- | --- |
| `clang-format` | `uv run tools/scripts/check.py format` | formatting against `.clang-format` |
| `clang-tidy` | `uv run tools/scripts/check.py tidy` | the check set in `.clang-tidy` |

If the hooks are not installed (`.git/hooks/pre-commit` missing) they will not
run at all. In that case run them yourself before committing:

```bash
lefthook run pre-commit   # or, if lefthook is missing: uv run tools/scripts/check.py format && uv run tools/scripts/check.py tidy
```

and tell the user they can install the hooks once per clone with
`brew install lefthook && lefthook install`.

**When a hook fails, fix the cause. Never use `git commit --no-verify`, and
never disable, weaken, or skip a check to get the commit through.**

- **clang-format** — reformat the staged changes and restage:
  ```bash
  "$(brew --prefix llvm)/bin/git-clang-format" --binary "$(brew --prefix llvm)/bin/clang-format" --staged && git add -u
  ```
  Then commit again.

- **clang-tidy** — read each diagnostic and fix the code properly: apply the
  suggested modernization, fix the real bug, tighten the type, remove or add the
  `#include` that `misc-include-cleaner` points at. If a function trips the
  cognitive-complexity or function-size limit, split it rather than raising the
  limit. A `// NOLINT` is acceptable only when the diagnostic is a genuine false
  positive — it must be narrow (single line or `NOLINTNEXTLINE`), name the exact
  check, and carry a comment explaining why. Flag any NOLINT you add in your
  final report.
  If it fails to configure or cannot find `compile_commands.json`, run
  `cmake --preset debug` and retry.

- **Missing tool** (`error: 'clang-format' not found` etc.) — this is an
  environment problem, not a code problem. Stop, leave the changes staged, and
  tell the user to run `brew install llvm` (or `brew install lefthook`).

After every fix: re-stage (`git add -u`, or `git add -A` if you created files)
and re-run the same `git commit -F -` call with the same message, updating the
body only if your fixes changed what the commit actually does.

Give up after **3** failed commit attempts: report what is still failing, with
the hook output, and leave the changes staged so the user can take over.

## 5. Report

Finish with:

```bash
git --no-pager log -1 --stat
git status --short
```

Then tell the user, briefly: the commit subject and short SHA, anything you had
to fix for the hooks, anything you deliberately left unstaged, and any NOLINT
you added.
