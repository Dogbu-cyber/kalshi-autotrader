# Structured Logging Plan (Datadog-friendly)

## Goals
- Non-blocking logging via async queue
- JSON logs to stdout (Datadog Agent collects)
- Minimal overhead in hot path

## Log shape (JSON)
- timestamp
- level (trace/debug/info/warn/error)
- service (kalshi-autotrader)
- env (prod/demo)
- ddsource (cpp)
- module (ws_client/feed_handler/parser)
- message
- fields (optional object)

## API
- Logger interface: `log(level, module, message, fields)`
- Concrete logger: `JsonLogger` (stdout)
- Optional: `NullLogger` for tests

## Runtime
- Single producer queue (ring buffer)
- Background thread flushes to stdout
- Drop policy: drop-oldest + counter

## Instrumentation
- ws connect/disconnect/reconnect
- subscribe success/error
- parse errors + unsupported types
- per-market sequence gaps

## Open questions
- Default log level?
- Queue size?
- Drop policy (drop-oldest vs drop-newest)?
- Include raw JSON on parse failure?

