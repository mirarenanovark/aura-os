# AuraOS Engineering Philosophy
**Status:** NORMATIVE · **Applies to:** every file, every commit, every agent.

The five laws, verbatim from Rafx:

> **Clean. Simple. Small. Fast. Direct.**
> **No AI bloat. No overengineering.**
> **Proven. Works. Easy to read. Follow.**
> **The whole architecture is supposed to be easy. Clean and easy to develop for.**

## How laws resolve conflicts (in order)
1. **Works** — a proven boring solution beats a clever fragile one.
2. **Simple / Clean** — if a module can't be understood in 5 minutes, rewrite it.
3. **Small / Fast** — budgets live in `docs/wiki/08-measurements/`; every claim needs evidence.
4. **Direct** — one obvious way to do a thing; no hidden layers, no indirection theater.

## Enforcement
* PRs violating budgets or introducing speculative abstraction are rejected.
* New dependency? Requires an ADR.
* New syscall/ABI surface? Requires a frozen contract in `03-contracts/` first.
* "Unsupported on this hardware profile" is a valid, clean answer.
