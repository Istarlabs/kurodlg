---
name: feature-workflow
description: >
  The mandatory recon → issue → branch → commit → PR workflow for every change in this repository.
  Use when starting any feature/fix, before writing new code of any kind, when checking whether
  something already exists in the codebase or in a shared library, creating a GitHub issue, creating
  a branch, committing, opening a pull request, or verifying a change before submission.
  Covers the mandatory pre-implementation recon checklist, branch naming,
  conventional commit format, the PR body template, stacked PRs, the build-only verification rule,
  and the local git/gh pager environment gotchas.
---

# Feature Workflow (Issue → Branch → PR)

Every change goes through a GitHub issue and a PR. **Never commit directly to the main branch.**

## Exception: genuinely small changes
An issue and a PR for a one-file CSS tweak or a docs refresh is more overhead than the change
itself, so the user waives the workflow for those and commits them straight to the main branch.
Applies when the change is small, self-contained and needs no review to be understood.

Never *assume* the waiver: do the work, then ask "master direkt oder Branch + PR?" and let the
user pick. If they say master, commit and offer the push — don't re-argue the workflow. Anything
touching entities, migrations, permissions or more than a couple of files goes the normal route.

## Everything on GitHub is written in English
Issue titles and bodies, PR titles and bodies, **PR comments and review replies**, commit
messages — all English, even when the conversation with the user runs in another language.
The only exception is an established term with no real equivalent in English — translating those
would obscure what the record is.

## Environment gotchas (this machine)
- Git and GitHub go through `Scripts/repo.py`; it handles the pager, the German git output and `--json`.
  `python3 Scripts/repo.py --help` before reaching for a raw `git`/`gh` line — a missing action is
  added to the script, not typed by hand.
- Interactive git flags (`-i`) don't work. `gh` is authenticated — no need to ask.

## 0. Recon — before writing a single line

The most common failure is rebuilding something that already exists three folders over. Recon is
not optional and it is **not** waived for small changes — small changes are exactly where it gets
skipped.

Answer all four, and **report the answers to the user before implementing** (two or three lines, not
a document). That is the cheapest possible moment for them to say "das gibt's schon in X".

1. **Does it already exist?** Search the *source*, in three places:
   - this repository — `python3 Scripts/find.py "concept" --text` for the concept and its synonyms,
     not just the exact name you'd pick
   - the shared libraries this repository references, in their own checkout
   - the reusable-component and helper projects of this solution
   A generated inventory is a starting point, never the verdict — regenerate it, then still confirm
   in the source.
2. **What is the nearest existing pattern?** Name the closest existing feature and follow its shape.
   Copying a sibling beats inventing a structure, and it keeps the codebase readable.
3. **Which integration points does this touch?** Walk the list explicitly, don't guess. The project's
   own `.claude/CLAUDE.md` and the appendix at the end of this skill name them.
4. **What is the smallest version that solves it?** If a rung above "write new code" holds, take it
   and say so.

For a wide sweep — "do we handle X anywhere?", spanning many folders and naming conventions — use
the `Explore` agent rather than pulling twenty files into context. Phrase the task as *"does X
already exist, and what is the nearest existing pattern"*, not just *"find X"*.

## 1. Issue first
- `python3 Scripts/repo.py issue-create --title "..." --label "..." --file body.md` — descriptive
  title, markdown body with `- [ ]` task lists, related issues linked. Missing labels first, with
  `repo.py label`.
- **It is assigned automatically**, and so is every pull request. An unassigned item on a
  one-maintainer repository reads as nobody's, which is never what it means. Anything that arrived
  another way: `python3 Scripts/repo.py assign`.
- Big topics: group related issues, close them from **one** PR (`Closes #A, closes #B`).

## 2. Branch
- Off the main branch: `python3 Scripts/repo.py branch feat/NNN-short-slug` (or
  `fix/NNN-short-slug`) — `NNN` = issue number, slug = kebab-case. It pulls first and refuses a
  dirty tree.
- Several branches to click through in one round: `repo.py test-branch test/topic feat/a fix/b`.
  **A test branch is never pushed** — everything in it is already on the branches it merged, so a copy
  on GitHub is a branch nobody reviews and nothing merges.

## 3. Implement + commit
- Conventional Commits, English: `feat(scope): summary (#NNN)` / `fix(scope): summary`.
- One logical change per commit; `docs(skills): ...` for skill updates is normal.
- **Never commit EF migrations** — the folder is git-ignored on purpose.

## 4. Verify — use the scripts, not ad-hoc command chains

```bash
python3 Scripts/dotnet.py verify          # build + tests + warnings on the lines you changed
python3 Scripts/dotnet.py packages --vulnerable
```

**`Scripts/dotnet.py` is the first choice for anything dotnet** — build, warnings, tests, packages,
project list, SDK info. Do not rebuild those out of `dotnet … | grep …`: the SDK output on this
machine is **German**, so hand-written greps for `error`/`warning` silently miss things, and every
unique command line costs a permission prompt. Counting warning lines that way hid a high-severity
package advisory (`NU1903`, no file position, never matched `warning CS`) and inflated the warning
count by counting each one once per referencing project.

- The build must be clean. `verify` reports warnings on **changed lines**, which is the signal;
  pre-existing ones in the same file are listed separately and are not yours to fix.
- New, changed or now-unused user-facing strings? `python3 Scripts/translations.py add|update|remove {Area}`
  (see the `localization` skill) — never the three manual steps.
- **Never install browsers/Playwright** — the user tests the UI manually.
- Non-trivial pure logic: leave a runnable check behind, mention the cases in the PR body.

### Noticing a third repetition
When you catch yourself running the same shape of command a third time, **ask whether to add it to
`Scripts/`** rather than building it silently. The user wants that small gate — it is their chance to
say the tool is aimed wrong before it becomes the documented path. Asking costs one sentence; a
wrong-shaped script costs every future use.

A script that belongs to every project goes in the shared devscripts checkout rather than in one
repository's `Scripts/`; a script about this project's domain stays here. Rules either way: absolute
paths derived from its own location, values that cannot drift (force `DOTNET_CLI_UI_LANGUAGE=en`,
read the project list from the `.sln`), refuse rather than half-apply, non-zero exit on failure.

## 5. Pull request
`python3 Scripts/repo.py pr-create --title "feat(scope): summary" --file body.md` — body template:

```markdown
Closes #NNN

## What
- Bullet list: entities/repos/UI/localization touched, key decisions inline

## Skipped (per issue)        <- only if something was deliberately left out
...

## Verified
- `dotnet build` clean
- <logic checks run, migration applied to dev DB (not committed)>

🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

- **Stacked PRs**: if the branch depends on an unmerged PR, base it on that branch and note
  "**Stacked on #X** — merge that first, then re-target to master" at the top of the body.
