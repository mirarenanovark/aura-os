# Workspace Environment Rules

- **No WSL**: Do not run WSL commands (`wsl`, etc.). There is no WSL environment configured or available here.
- **Build Environment**: AuraOS is built on a remote Linux machine (cross-compiler toolchain, grub-mkrescue, etc.). Do not attempt local Linux builds or invoke WSL locally.
- **Testing**: Testing and ISO execution are performed locally on Windows using QEMU via `testing/run_qemu.bat` or `testing/run_qemu.ps1`.
