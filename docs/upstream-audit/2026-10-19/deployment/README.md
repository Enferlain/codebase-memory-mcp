# Installed migration receipt — 2026-10-19

The tested upstream v0.12.1 plus P1-P5 build is installed as
`/home/imi/.local/bin/codebase-memory-mcp` (`0.12.1-fork.1`). SHA-256:
`4962cf320c2345cf47b7871fd5f1a14107aecf13e77a86b1ffd35689888ea026`.
Source acceptance commit: `f0379962`; canonical checkout:
`/home/imi/Projects/codebase-memory-mcp`, branch `fork/v0.12.1-patches`.

The guarded installer stopped the old frontends/daemon, replaced the executable,
and discarded 21 old project indexes. The old checkout's build was removed,
the tested build moved into the canonical checkout, and the separate
`codebase-memory-mcp-v0121` worktree removed. No binary or index backup was
created, as requested. The old fork remains in Git and on origin.

The install used `install --force --skip-config --reset-indexes -y`.
Client integration files and CBM settings were preserved. The installer also
added the standard `.local/bin` PATH entry to `.bashrc`. Seven orphaned old
`*.db.stage.*` files were discarded before the new index jobs started.
Existing MCP clients must reconnect/restart: their previous transports were
retired during installation. A fresh MCP frontend was verified successfully.

## Rebuilt indexes

All 18 existing source roots rebuilt successfully with the installed executable
using full indexing, without the old graph databases. Semantic index version:
`1000009`. Three old index roots no longer exist and were not recreated:

- `/home/imi/obsidian-vault-2026`
- `/tmp/hcom-p33-live-fixed.1uwl7J`
- `/tmp/hcom-p33-live.ObSqOC`

Successful indexing does not imply exhaustive parsing or graph coverage. The
reported skipped/partial counts below remain visible for follow-up; the CBM
repository includes unsupported test declaration forms in its skipped records.

| Source root | Nodes | Edges | Skipped records | Partial parses |
| --- | ---: | ---: | ---: | ---: |
| `/home/imi/Projects/codebase-memory-mcp` | 80704 | 405404 | 50 | 132 |
| `/home/imi/Projects/sd-scripts` | 15282 | 65213 | 0 | 2 |
| `/home/imi/Projects/hcom` | 9668 | 60260 | 0 | 2 |
| `/home/imi/.codex/memories` | 176 | 201 | 0 | 0 |
| `/home/imi/.config/hcom-model-router/real-task-test/workspace` | 33 | 83 | 0 | 0 |
| `/home/imi/.config/hcom-model-router/scratch` | 2 | 1 | 0 | 0 |
| `/home/imi/Projects/codex-setup` | 464 | 937 | 0 | 0 |
| `/home/imi/Projects/hermes-gate` | 213 | 242 | 0 | 0 |
| `/home/imi/Projects/hermes-llm-usage` | 274 | 685 | 0 | 0 |
| `/home/imi/Projects/review-mcp` | 250 | 1065 | 0 | 0 |
| `/mnt/d/Projects/tag-workspace` | 399 | 1598 | 16 | 0 |
| `/home/imi/Projects/finance-sync-mcp` | 462 | 983 | 0 | 0 |
| `/home/imi/Projects/hermes-agora` | 1987 | 8743 | 0 | 0 |
| `/mnt/d/Projects/image-downloaders/gallery-dl` | 9409 | 43318 | 0 | 3 |
| `/home/imi/Projects/data-processing` | 6938 | 25634 | 0 | 14 |
| `/mnt/d/Projects/image-downloaders/imgbrd-grabber` | 9633 | 33212 | 0 | 325 |
| `/mnt/d/ComfyUI-current` | 618459 | 707923 | 0 | 20 |
| `/home/imi/Projects/pytortch-backend-experiments` | 574073 | 684921 | 0 | 611 |

## Installed runtime checks

A fresh stdio MCP process initialized as `0.12.1-fork.1` and exposed 17 tools.
Against the rebuilt, live sd-scripts index, both the training and token-cache
contract calls resolved to their base contracts with `lsp_method` evidence.
The depth-two CALLS/OVERRIDE topology contained SD, SDXL, and SD3 implementations.
Exact source snippets were readable; coverage for `training_loop.py` reported
no recorded issue with matching metadata (a best-effort signal, not proof of
complete coverage). Saved index status was `ready`. The daemon fingerprint
matched the installed bytes, `4962cf320c23...`.

These are post-install checks on Linux/glibc. The earlier isolated tests,
watcher checks, regression suites, review dispositions, and remaining platform
limits are recorded in the parent migration report.

## Evidence

- [Machine-readable receipt](receipt.json)
- [Installer output](install.log)
- [Reindex progress](reindex.log) and [per-project results](reindex-results.json)
- [Fresh MCP smoke results](mcp-smoke.json)
- [Compressed raw index responses and MCP session evidence](raw-index-and-mcp-evidence.json.gz)

Only operation metadata and test evidence are retained here; these are not
backups of the discarded binaries or graph databases.
