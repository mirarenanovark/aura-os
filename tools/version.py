#!/usr/bin/env python3
"""AuraOS version gate — run before every push and at every stage close.

Usage:
  tools/version.py check              # full gate; exit 1 on any violation
  tools/version.py bump module <dir> <part>   # e.g. bump module kernel minor
  tools/version.py bump stage <new-version>   # e.g. bump stage 0.2.0-fb
Clean. Simple. Direct. No dependencies beyond git + stdlib.
"""
import re, subprocess, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
MODULES = ["boot", "kernel", "drivers", "gui", "libs", "userland", "tools", "tests"]
SEMVER = re.compile(r"^(\d+)\.(\d+)\.(\d+)(?:-([a-z0-9.\-]+))?$")


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True).stdout.strip()


def read_version(path: pathlib.Path) -> str:
    return path.read_text().strip() if path.exists() else ""


def fail(msg: str) -> int:
    print(f"VERSION GATE FAIL: {msg}")
    return 1


def cmd_check() -> int:
    rc = 0
    version = read_version(ROOT / "VERSION")
    if not SEMVER.match(version):
        return fail(f"VERSION '{version}' is not semver (MAJOR.MINOR.PATCH[-stage])")
    tag = f"v{version}"
    if tag not in git("tag", "--list").splitlines():
        rc = fail(f"tag {tag} missing — tag the release commit")
    log = pathlib.Path(ROOT / "docs" / "CHANGELOG.md")
    if not log.exists() or version not in log.read_text():
        rc = fail(f"docs/CHANGELOG.md has no entry for {version}")
    # every module needs a MODULE_VERSION file
    for m in MODULES:
        mp = ROOT / m / "MODULE_VERSION"
        mv = read_version(mp)
        if not mv:
            rc = fail(f"{m}/MODULE_VERSION missing/empty")
        elif not SEMVER.match(mv):
            rc = fail(f"{m}/MODULE_VERSION '{mv}' is not semver")
    # any commit touching a module must bump that module (check last commit)
    changed = git("diff", "--name-only", "HEAD~1", "HEAD").splitlines()
    touched = {c.split("/")[0] for c in changed if c.split("/")[0] in MODULES}
    bumped = {c.split("/")[0] for c in changed if c.endswith("MODULE_VERSION")}
    for m in touched - bumped:
        rc = fail(f"commit changes '{m}/' without bumping {m}/MODULE_VERSION")
    is_release = any(c == "docs/CHANGELOG.md" for c in changed) or version in git("show", "--stat", "--format=%s", "HEAD")
    if touched and not is_release and len(touched - bumped) == 0 and not changed:
        pass  # normal feature commit with proper bumps
    print("VERSION GATE OK" if rc == 0 else "see failures above")
    return rc


def cmd_bump(args: list[str]) -> int:
    if len(args) >= 3 and args[0] == "module":
        _, mod, part = args
        mp = ROOT / mod / "MODULE_VERSION"
        mv = read_version(mp)
        if not SEMVER.match(mv):
            return fail(f"{mod}/MODULE_VERSION '{mv}' unreadable")
        new = _bump(mv, part)
        mp.write_text(new + "\n")
        print(f"{mod}: {mv} -> {new}\nnow commit: git add {mod}/ && git commit -m 'chore({mod}): bump to {new}'")
        return 0
    if len(args) >= 2 and args[0] == "stage":
        new = args[1]
        if not SEMVER.match(new):
            return fail(f"'{new}' is not semver")
        (ROOT / "VERSION").write_text(new + "\n")
        print(f"VERSION -> {new}\nrelease commit:\n"
              f"  git add VERSION docs/CHANGELOG.md\n"
              f"  git commit -m 'chore(release): v{new}'\n"
              f"  git tag v{new}\n"
              f"  git push origin main --tags")
        return 0
    print(__doc__)
    return 1


def _bump(mv: str, part: str) -> str:
    m = SEMVER.match(mv)
    if not m:
        return mv
    maj, mi, pa = int(m.group(1)), int(m.group(2)), int(m.group(3))
    if part == "major":
        return f"{maj+1}.0.0"
    if part == "minor":
        return f"{maj}.{mi+1}.0"
    if part == "patch":
        return f"{maj}.{mi}.{pa+1}"
    return mv


if __name__ == "__main__":
    args = sys.argv[1:]
    sys.exit(cmd_check() if not args or args[0] == "check" else cmd_bump(args))
