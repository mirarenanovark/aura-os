# Project Aeris — Engineering Philosophy & Core Laws

The codebase follows an uncompromising philosophy. Every patch, every module, and every architectural decision is measured against these five laws. If code violates them, it gets rewritten — not refactored.

## The Five Laws

1. **Clean** — One file, one clear purpose. Consistent naming, consistent formatting, no surprises. If a newcomer can't map a directory to a concept in 30 seconds, the structure is wrong.
2. **Simple** — The straightforward solution beats the clever one. No abstraction layers that serve only future hypotheticals. No design patterns for their own sake. Boring code is correct code.
3. **Small** — The entire OS targets a codebase you can read in a weekend. Kernel core stays minimal; features ship as small isolated modules, not accreted monoliths.
4. **Fast** — Direct paths only. No indirection theater, no deep call stacks, no hidden allocations. Measure, don't guess. A syscall should be as close to the metal as the design allows.
5. **Direct** — Say what you mean. No wrappers around wrappers, no generic frameworks where a function suffices. The code reads top-to-bottom like a well-written document.

## What We Reject
* **AI bloat** — No machine-generated over-abstraction, comment noise, defensive try/catch pyramids, or speculative APIs. Every line must earn its place.
* **Overengineering** — No plugin systems for two users, no config languages where a #define works, no dependency injection where a direct call works.
* **Unproven techniques** — We use what has worked for decades: freestanding C, flat memory, direct hardware access, ring buffers, fixed arrays. Novelty is a cost, not a feature.
* **Hidden magic** — No macros that hide control flow, no generated code, no build-time tricks. What you see is what runs.

## Development Rules
1. **Easy to read beats easy to write.** Code is read 100x more than it is written.
2. **Easy to develop for.** A new contributor builds the OS from clone to boot in under 10 minutes with one script and one README.
3. **Small modules, clear borders.** If a file exceeds ~500 lines or serves two purposes, split it.
4. **No dependencies without proof of necessity.** Every external library must be small, auditable, and essential (e.g. Limine, stb_truetype).
5. **The architecture is the API.** Simple, documented, direct. A developer writing an app for Aeris should need to learn exactly three things: the syscall table, the window server protocol, and libaeris-ui.

## The Test
Before any commit, ask:
> *Is this the smallest, clearest, fastest, most direct way to do this?*

If the answer is no, delete it and start over. Complexity is the enemy. Clarity is the product.
