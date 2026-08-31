# Using the Local Fork from Other Agent Environments

This is the living setup guide for exposing the locally built
`codebase-memory-mcp` fork to Codex, Claude Code, Antigravity/`agy`, Hermes, and
other MCP clients on the same WSL/Linux account.

Last verified: **2026-08-31**

- Binary: `/home/imi/.local/bin/codebase-memory-mcp`
- Expected SHA-256: `0c261f40a546a64148a40938b588a05394b64d94b45ad2367f9b2e81b8fdc419`
- Source branch: `fork/usable-graph-refresh`
- Validated code commit: `3814d323`

See [FORK_UPSTREAM_GAPS.md](FORK_UPSTREAM_GAPS.md) for the fixes and known
limitations carried by this build.

## Important model

This is a local stdio MCP server, not an HTTP URL. Each agent launches the same
absolute executable. Those frontends coordinate through one per-account daemon
and the same graph cache, normally `~/.cache/codebase-memory-mcp`.

All active clients must use the exact same binary bytes, coordination ABI, and
canonical `CBM_CACHE_DIR`. A client pointed at an old build will be refused
rather than silently sharing incompatible state.

The `/home/imi/...` path works only for clients running as this user inside the
same WSL/Linux environment. A native Windows client, container, remote host, or
different account needs a compatible binary and its own absolute path. Do not
expect a Windows process to execute this Linux ELF directly or a remote client
to see this account's graph cache automatically.

## Verify the installed build

```bash
command -v codebase-memory-mcp
sha256sum /home/imi/.local/bin/codebase-memory-mcp
/home/imi/.local/bin/codebase-memory-mcp --version
/home/imi/.local/bin/codebase-memory-mcp daemon status
```

The checksum should match the value at the top of this file. At this snapshot,
the daemon reports build `dev (0c261f40a546...)`.

Current machine state:

| Client | Local fork registered? | Configuration checked |
| --- | --- | --- |
| Codex | Yes | `/home/imi/.codex/config.toml` |
| Claude Code | Yes | `/home/imi/.claude.json` |
| Antigravity / `agy` | Yes | `/home/imi/.gemini/config/mcp_config.json` |
| Hermes | No | `/home/imi/.hermes/config.yaml` |

## Generic JSON configuration

Use this in clients whose MCP configuration has an `mcpServers` object:

```json
{
  "mcpServers": {
    "codebase-memory-mcp": {
      "command": "/home/imi/.local/bin/codebase-memory-mcp",
      "args": []
    }
  }
}
```

Merge the entry into the existing object; do not replace other servers. Start a
new agent session after changing the file. Existing sessions generally do not
discover newly configured tools dynamically.

## Codex

User config: `$CODEX_HOME/config.toml`, normally `~/.codex/config.toml`.

```toml
[mcp_servers.codebase-memory-mcp]
command = "/home/imi/.local/bin/codebase-memory-mcp"
args = []
```

This machine is already configured at `/home/imi/.codex/config.toml`. Restart
Codex after editing or replacing the binary.

## Claude Code

Use the generic JSON entry in either:

- `~/.claude.json` for this user's sessions; or
- `.mcp.json` in a repository for project-local configuration.

This machine's `~/.claude.json` is already pointed at the local binary. Restart
Claude Code, then use `/mcp` to confirm the server and its graph tools are
present.

An `hcom claude` launch does not proxy MCP itself: the launched Claude client
still needs to read a Claude configuration containing this entry.

## Antigravity and `hcom run agy`

Antigravity uses:

```text
~/.gemini/config/mcp_config.json
```

Put the generic JSON entry there. On this machine the exact file is
`/home/imi/.gemini/config/mcp_config.json` and is already configured.

`hcom run agy` launches the Antigravity client, so a newly launched worker uses
that same Gemini/Antigravity MCP configuration for its effective home. If an
isolated profile changes `HOME` or `GEMINI_CLI_HOME`, put the file under that
profile's effective `.gemini/config/` directory instead. Stop and relaunch the
worker after changing it; an already running worker will not gain the tools.

`GEMINI.md` controls when the model prefers graph discovery; the MCP JSON is
what makes the tools callable. Both are useful, but they solve different parts
of the integration.

## Hermes

Hermes is **not currently configured** with this server on this machine. Its
existing config contains a safety rule that blocks the broad CBM `install`
command, so add only this server explicitly:

```bash
hermes mcp add codebase-memory-mcp \
  --command /home/imi/.local/bin/codebase-memory-mcp
```

Equivalent YAML under `$HERMES_HOME/config.yaml` (normally
`~/.hermes/config.yaml`) is:

```yaml
mcp_servers:
  codebase-memory-mcp:
    command: /home/imi/.local/bin/codebase-memory-mcp
    args: []
    enabled: true
```

Merge that mapping with the existing `mcp_servers`; do not replace the other
entries. Restart Hermes and confirm the discovered `mcp_...` tools before
relying on them.

## Other MCP clients

For a JSON-based client, start with the generic entry above and use the client's
documented user or project MCP file. The upstream README lists common locations,
including:

| Client | Typical configuration |
| --- | --- |
| Gemini CLI | `~/.gemini/settings.json` |
| OpenCode | `$OPENCODE_CONFIG` or its global config |
| VS Code Copilot | platform `Code/User/mcp.json` |
| Cursor | `~/.cursor/mcp.json` |
| Windsurf | `~/.codeium/windsurf/mcp_config.json` |
| Kiro | `$KIRO_HOME/settings/mcp.json` |
| Qwen Code | `~/.qwen/settings.json` |
| GitHub Copilot CLI | `$COPILOT_HOME/mcp-config.json` |
| Kimi Code CLI | `$KIMI_CODE_HOME/mcp.json` |

Client schemas differ. Preserve the surrounding file format and existing
servers; only the executable path and empty argument list are universal.

For an ephemeral container or remote worker, mounting only the host socket is
not a supported substitute for installing a compatible binary. Either run the
agent in this same environment or copy/build the fork for the target and let it
use a separate cache.

## Give agents a useful operating policy

MCP registration exposes tools, but it does not guarantee that an agent will
prefer them. Put a short policy in that client's durable instruction file:

1. At session start, use `list_projects` or `index_status` to identify the
   current project and generation.
2. Prefer `search_graph`, then `trace_path`, then `get_code_snippet` for
   structural code discovery.
3. Use exact qualified names after resolving ambiguity.
4. Call `check_index_coverage` for every material evidence path before negative
   or exhaustive claims; inspect any reported gaps in source.
5. Fall back to `rg` for literals, non-code files, or graph gaps.

The complete maintained policy lives in this repository's integration
templates. Manual MCP registration does not install those skills, hooks, or
agent definitions automatically.

## Index and query without an agent

The one-shot CLI is useful for confirming that a new environment sees the same
binary and graph data:

```bash
codebase-memory-mcp cli list_projects
codebase-memory-mcp cli index_repository --repo-path /absolute/path/to/repo
codebase-memory-mcp cli search_graph \
  --project project-name \
  --name-pattern '.*Handler.*' \
  --label Function
codebase-memory-mcp cli trace_path \
  --project project-name \
  --function-name exact.qualified.name \
  --direction both
```

For list-valued CLI arguments, repeat the flag:

```bash
codebase-memory-mcp cli trace_path \
  --project project-name \
  --function-name exact.qualified.name \
  --edge-types CALLS \
  --edge-types OVERRIDE
```

The CLI takes a crash-safe build lease but does not join or start the persistent
coordination daemon. Graph mutations still use shared project locks.

## Safely install a newer local build

Do this only after the new build and tests pass:

1. Close Codex, Claude, Antigravity, Hermes, and any other CBM-backed sessions.
2. Stop the daemon:

   ```bash
   /home/imi/.local/bin/codebase-memory-mcp daemon stop
   ```

3. Replace `/home/imi/.local/bin/codebase-memory-mcp` with the validated binary
   using an atomic same-filesystem rename or the repository's supported
   activation flow.
4. Confirm the new checksum.
5. Start the daemon or let the first MCP client start it:

   ```bash
   /home/imi/.local/bin/codebase-memory-mcp daemon start
   ```

6. Restart each client and confirm its tools.
7. Update the checksum, commit, and validation date in both living documents.

Do not overwrite the binary while old clients are alive. Do not point some
clients at `build/c/codebase-memory-mcp` and others at the installed path.

For this modified fork, prefer the explicit per-client entries above. The
upstream `install` command configures many detected clients, hooks, skills, and
instruction files; that is broader than merely registering this local server.
Use its dry-run/audit facilities only when that broader integration is actually
intended.

## Environment variables

Usually no environment block is needed. If one is added, keep daemon-wide
values consistent in every client:

| Variable | Use |
| --- | --- |
| `CBM_CACHE_DIR` | Moves graph/config storage from `~/.cache/codebase-memory-mcp`; all active clients must resolve to the same canonical directory. |
| `CBM_RUNTIME_DIR` | Moves the private daemon rendezvous directory when the default `/tmp` ancestry is unsuitable; use the same value everywhere. |
| `CBM_ALLOWED_ROOT` | Restricts a particular MCP session to repositories below one root; this may intentionally differ per client. |
| `CBM_LOG_LEVEL` | Controls logging; daemon-owned settings come from the client that starts the daemon. |

When changing daemon-owned values, close all clients, stop the daemon, update
every relevant config, and start a fresh session.

## Troubleshooting

### The client shows no tools

- Confirm the absolute binary exists and is executable.
- Validate the client's JSON/TOML/YAML rather than replacing the whole file.
- Restart the entire client, not only the conversation.
- Run `daemon status` and inspect
  `~/.cache/codebase-memory-mcp/logs/cbm-daemon.log`.

### The build is rejected as incompatible

Another client or CLI command is using different binary bytes or a different
cache root. Close every CBM-backed client, wait for commands to finish, stop the
daemon, verify that no old CBM process remains, and restart from the one intended
binary. Do not delete rendezvous files while a live daemon exists.

### The repository is missing

Run `list_projects`, then index the absolute repository path once. Auto-sync and
watchers maintain a known project after that. A clean coverage result means only
that no gap was recorded; it is not proof of complete extraction.

### Different environments see different projects

Check the effective user, `HOME`, `CBM_CACHE_DIR`, and platform. Clients in the
same WSL account normally share the default cache. Containers, native Windows,
SSH hosts, and isolated profiles normally do not.

## Maintenance checklist

When a new client is added or the fork build changes:

1. Verify its effective home and MCP config path.
2. Point it at the one installed absolute binary.
3. Preserve existing servers and environment settings.
4. Restart and confirm tool discovery.
5. Run `list_projects` and one exact `search_graph` query.
6. Confirm `daemon status` reports the expected build.
7. Record the client and any special profile path in this document.
