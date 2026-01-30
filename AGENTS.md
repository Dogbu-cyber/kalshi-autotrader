# AGENTS.md

## Purpose
This repo builds a Kalshi market-data research platform in C++23. It prioritizes modular design, defensive coding, and production-minded structure.

## Quickstart
- Build: `./scripts/build.sh`
- Run: `./scripts/run.sh`
- Clang-tidy: `./scripts/clang_tidy.sh`

## Architecture (current)
- `include/kalshi/core/` — auth/config/endpoints
- `include/kalshi/logging/` — async JSON logger + log types
- `include/kalshi/md/model/` — types + market event structs + sink concept
- `include/kalshi/md/parse/` — parsing helpers, JSON field constants
- `include/kalshi/md/protocol/` — message type constants + subscribe protocol
- `include/kalshi/md/ws/` — websocket client + constants
- `include/kalshi/md/` — dispatcher + feed handler
- `src/kalshi/` — core implementations
- `src/kalshi/md/` — md implementations

## Conventions
- Rust-style naming: constants in UPPER_SNAKE, variables snake_case.
- Defensive coding with `std::expected` and explicit validation.
- Prefer small, single-responsibility functions.
- Avoid implicit defaults in config; `config.json` is source of truth.

## Config
- `config.json` must include:
- `env`
- `ws_url`
- `subscription.channels`
- `subscription.market_tickers` (required for `ORDERBOOK_DELTA`)
- `logging.*` optional (defaults applied if omitted)
- `logging.output_path` optional (defaults applied if omitted)
- `logging.log_raw_messages` optional (defaults applied if omitted)
- `output.*` optional (defaults applied if omitted)

## Secrets
- Do NOT commit secrets. `pkey.pem` should remain local.
- Env vars:
  - `KALSHI_API_KEY`
  - `KALSHI_PRIVATE_KEY_PATH` (or `KALSHI_PRIVATE_KEY`)

## Output
- Raw WS messages are written to `output.raw_messages_path` (one JSON object per line).

## Planning
- `planning/` is for design docs and scratch notes (gitignored).

## Current focus
- Core event parsing + dispatch
- Orderbook state tracking + sequence validation
- Structured logging (Datadog JSON)
