3. Run the following commands in parallel:
   - Add relevant untracked files to the staging area.
   - Create the commit with a message${COMMIT_MESSAGE_ENDING_CLAUSE}
   - Run git status after the commit completes to verify success.
   Note: git status depends on the commit completing, so run it sequentially after the commit.

- In order to ensure good formatting, ALWAYS pass the commit message via a HEREDOC, a la this example:
<example>
git commit -m "$(cat <<'EOF'
   Commit message here.${COMMIT_ATTRIBUTION_TEXT}
   EOF
   )"
</example>

2. Stage the relevant files and create the commit. To ensure good formatting, ALWAYS pass the commit message inline via a ${IS_BASH_ENV_FN()?"HEREDOC":"here-string"}, never from a file or template (`-F`, `--file` and `-t` are refused while this skill runs):
${IS_BASH_ENV_FN()?````
git commit -m "$(cat <<'EOF'
Commit message here.${COMMIT_ATTRIBUTION_TEXT?`

${COMMIT_ATTRIBUTION_TEXT}`:""}
EOF
)"
``````:````
git commit -m @'
Commit message here.${COMMIT_ATTRIBUTION_TEXT?`

${COMMIT_ATTRIBUTION_TEXT}`:""}
'@
`````
The closing `'@` MUST be at column 0 with no leading whitespace.`}

2. Create a single commit with an appropriate message${COMMIT_ATTRIBUTION_TEXT?", ending with the attribution text shown in the example below":""}, passed inline as shown (`-F`/`--file` is refused while this skill runs):
${IS_BASH_ENV_FN()?````
git commit -m "$(cat <<'EOF'
Commit message here.${COMMIT_ATTRIBUTION_TEXT?`

${COMMIT_ATTRIBUTION_TEXT}`:""}
EOF
)"
````:````
git commit -m @'
Commit message here.${COMMIT_ATTRIBUTION_TEXT?`

${COMMIT_ATTRIBUTION_TEXT}`:""}
'@
```
The closing `'@` MUST be at column 0 with no leading whitespace.`}

### Attribution (Commits & PRs)
```json
{
  "attribution": {
    "commit": "Custom commit trailer text",
    "pr": "Custom PR description text"
  }
}
```
Set `commit` or `pr` to empty string `""` to hide that attribution. To hide all of it, set both to `""` and also set `"sessionUrl": false`. Write this object form, not `"attribution": false`: older Claude Code versions reject true or false here and then skip the whole settings file.

## Commit workflow for AI agents

Use conventional commit format, following [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/): `<type>[optional scope][optional !]: <description>`.

1. Inspect `git status --short`, `git diff` and `git diff --cached` to understand the working tree, the staged changes and the intended commit scope.
2. Review `git log -10 --format='%s'` for recent naming conventions. When useful, inspect related history with `git log -5 --format='%s' -- <affected paths>` or search commit messages with `git log --oneline --grep='<topic>'`. Use history to choose consistent scopes and terminology; base the message on the actual diff.
3. Choose a lowercase type: `feat` for a new feature, `fix` for a bug fix, `docs` for documentation, or `refactor`, `perf`, `test`, `build`, `ci`, `style` or `chore` when appropriate. Use a short scope for the affected subsystem when it adds useful context.
4. Write a concise English description in the imperative, without a trailing period. For example: `docs(agents): require conventional commit messages`. Add a body only when it helps explain the reason or consequences of the change. Separate the body and any trailers with blank lines. Mark breaking changes with `!` before the colon or a `BREAKING CHANGE:` footer explaining the incompatibility.
5. Preserve the user's staged selection and stage only additional files relevant to the authorized task, using explicit paths. Review `git diff --cached` and run `git diff --cached --check`, plus any checks appropriate to the changes, before committing.
6. Complete staging before creating the commit, then run `git status --short` after the commit succeeds. These dependent operations must run sequentially; only independent inspections may run in parallel.

Pass the message inline through a quoted HEREDOC on Bash or Zsh, as required above. Use concrete text; do not copy unresolved template variables or invent attribution trailers.

```bash
git commit -m "$(cat <<'EOF'
docs(agents): require conventional commit messages
EOF
)"
```
