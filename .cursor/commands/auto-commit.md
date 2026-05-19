# Auto-commit (Firelands)

Create one or more **Conventional Commits** from the current working tree. Apply **version bumps** when the diff requires them. Split unrelated changes into **separate commits**. Do not ask for confirmation unless something is ambiguous or unsafe.

Run only when the user invoked `/auto-commit` or explicitly asked for this workflow.

## Safety

- **Do not push** unless the user also asked to push.
- **Do not** amend, rebase, force-push, or use other destructive git commands.
- **Never** update git config; **never** skip hooks (`--no-verify`).
- **Never** compile or run tests unless the user authorized it in this session.
- **Never** stage secrets or local artifacts (see [Never commit](#never-commit)).

## Step 1 — Gather state (parallel)

```bash
git status --short
git diff
git diff --cached
git log -10 --oneline
```

If the tree is clean, report that and stop.

## Step 2 — Version bumps (before staging/commits)

Apply only what the diff needs. Stage bump files with the commit group they belong to.

### 2a. SQL schema and bundles

**Trigger:** Any change under `sql/migrations/`, `sql/init/`, or hand-edited `sql/bundled/` (except `zz_*.sql`).

1. Run (no build required):

   ```bash
   python3 tools/merge_migrations.py
   ```

2. Include regenerated outputs in the same commit as the SQL source changes:
   - `sql/bundled/firelands_auth.sql`, `firelands_characters.sql`, `firelands_world.sql` (when merge rewrites them)
   - `sql/bundled/zz_seed_schema_migrations.sql`

3. **New migration file:** Use the next numeric prefix after the highest `sql/migrations/<N>_*.sql` (currently check with `ls sql/migrations`). Do not renumber existing files.

4. **`db_version` in world init:** If `sql/init/world_schema.sql` gains or changes world DDL (tables/columns/indexes), update the `INSERT` into `version`:

   ```sql
   INSERT IGNORE INTO `version` (`core_version`, `db_version`) VALUES ('Firelands 4.3.4.15595', 'Schema YYYY-MM-DD');
   ```

   Use **today’s date** in UTC or the user’s local date if obvious from context. Re-run `python3 tools/merge_migrations.py` after editing init.

**Do not bump:** `CLIENT_VERSION_*` in `src/shared/Common.h` — that is the WoW **client** build (4.3.4.15595), not the emulator release.

### 2b. Pinned third-party versions

**Trigger:** `CMakeLists.txt` changes a `FetchContent` URL/tag (StormLib, googletest, spdlog, MariaDB, etc.).

Update the matching row in `docs/EN/STORM_LIB_ROADMAP.md` § “Pinned versions log” (and `docs/ES/` mirror only if that file was already part of the change set). Commit doc bump with the `build` or `chore` commit for CMake.

### 2c. No semver package

This repo has **no** `package.json` / `VERSION` file. Do not invent a project semver unless the user adds one later.

## Step 3 — Group changes by subject

Partition **every** changed path (staged and unstaged) into groups. Each group → **one** commit.

### Grouping rules

1. **Same feature / layer** — domain + application + infra adapters for one capability together; tests with the code they cover.
2. **Same conventional type** — do not mix unrelated `feat` and `fix` unless inseparable.
3. **SQL** — migrations + merge output + `zz_seed` in one `feat(db)` or `fix(db)` commit when one schema change.
4. **Docs-only** — `docs/`, `AGENTS.md`, `.cursor/` → `docs` or `chore`, separate from runtime code when possible.
5. **Tooling** — `tools/`, `cmake/`, `.github/` → `chore` or `build`, not mixed with gameplay unless trivial.
6. When unsure, **prefer two commits** over one muddy commit.

### Scope hints (match recent `git log`)

| Area | Scope |
|------|--------|
| World server / packets / session | `world` |
| Auth server | `auth` |
| SQL / migrations / bundled schema | `db` |
| Domain models / ports | `domain` |
| MySQL adapters | `infra` or `persistence` |
| Unit/integration tests | `test` |
| Documentation | `docs` (or omit scope) |
| Cursor rules/commands | `chore` |

## Step 4 — Commit message format

**Subject:** `<type>(<scope>): <imperative summary>`

| Type | Use when |
|------|----------|
| `feat` | New behavior or capability |
| `fix` | Bug fix |
| `refactor` | Behavior-preserving restructure |
| `perf` | Performance improvement |
| `docs` | Documentation only |
| `test` | Tests only |
| `chore` | Maintenance, tooling |
| `build` | CMake / dependencies |
| `ci` | CI configuration |
| `style` | Formatting only |
| `revert` | Reverts a prior commit |

**Rules:**

- English only; imperative mood (“add”, not “added”).
- Subject ≤ 72 characters; no trailing period.
- **Body** when *why* is not obvious, or for breaking changes / security / migrations.
- Optional footer: `Refs #123` if issue id is known from branch/context.
- No AI attribution in messages.

## Step 5 — Create commits (sequential)

For **each** group, sensible order (schema/deps → code → docs):

1. Stage **only** that group: `git add -- <paths…>`. Never `git add -A` across unrelated groups.
2. Verify: `git diff --cached --stat`
3. Commit:

```bash
git commit -m "$(cat <<'EOF'
<type>(<scope>): <subject>

Optional body.

EOF
)"
```

4. If a **pre-commit hook** fails: fix if straightforward, then **new** commit for that group (do not amend unless user rules allow).

5. Final: `git status` and summary.

### Staging conflicts

If the index mixes unrelated files, unstage per file (`git restore --staged <file>`) — do **not** `git reset --hard`.

## Never commit

- `.env`, credentials, private keys, local DB dumps
- `build/`, `logs/`, `node_modules/`, editor junk
- Accidental `firelands-cata-ref/` or `firelands-core-ref/` paths if present in the workspace

## Ambiguity

- **Single trivial change:** one commit, no questions.
- **Mixed large unrelated changes:** list a short plan, then execute.
- **Cannot classify:** one `chore` commit or ask once — never guess secrets.

## Output to user

Report:

- Version bumps applied (merge-migrations, `db_version`, docs pins) or “none”
- Number of commits created
- One line each: `<hash> <subject>` (N files)
- Anything left unstaged and why
- Reminder: push not performed unless requested
