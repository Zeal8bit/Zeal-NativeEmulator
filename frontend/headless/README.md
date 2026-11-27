# zeal-headless

## Install the agent skill

Paste the prompt for your agent to install the skill across your projects.

**Codex**

```text
Install the zeal-headless skill for my user account. Run zeal-headless --skill and save its output to ~/.agents/skills/zeal-headless/SKILL.md, creating the directory if needed.
```

**Claude Code**

```text
Install the zeal-headless skill for my user account. Run zeal-headless --skill and save its output to ~/.claude/skills/zeal-headless/SKILL.md, creating the directory if needed.
```

**Other terminal agents**

```text
Run zeal-headless --skill. If you support user-level SKILL.md skills, install the output as the zeal-headless skill in your documented user skill directory. Otherwise, use the output as instructions for this session.
```

For a project-only install, open the agent in the project root, change “for my user account” to “for this project”, and remove `~/` from the destination. If using a local build, replace `zeal-headless` in the command with the path to the executable, such as `./build-headless/zeal-headless` from the emulator repository root. Paste the prompt again after upgrading to refresh the skill.

See the [headless console guide](../../README.md#headless-console) for console and debugger commands.
