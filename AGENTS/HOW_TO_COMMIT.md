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