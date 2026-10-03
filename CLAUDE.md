# Claude Code instructions

`AGENTS.md` is the canonical instruction set for every agent. The files are
imported here so Claude receives their rules at the start of a session. Do not
repeat those rules in this file.

@AGENTS.md
@code/AGENTS.md
@manual/AGENTS.md

## Writing-rule hook

`.claude/settings.json` runs the shared `.agents/hooks/style-rules.py` on
five events. Edit, Write, and NotebookEdit payloads are checked as they land.
Markdown edits re-inject the writing rules from `AGENTS.md`; C and C++ edits
under `code/` re-inject the comment rules from `code/AGENTS.md` when they
touch comments and report objective violations. Subagents receive both rule
sections at start, and compaction re-injects them. Shell and script writes are
not checked while they run: SessionStart records a worktree baseline, and Stop
diffs the tree against it and reports what changed. That scan covers the whole
worktree, so edits another session or an editor makes in the same tree land in
its report too. No hook blocks. Edit results are context for the agent, and
the end-of-turn scan is a message to the user. Codex uses the same script
through `.codex/hooks.json` with the per-edit subset; the `AGENTS.md` files
remain the only source of the rules.
