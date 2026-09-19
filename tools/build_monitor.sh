#!/usr/bin/env bash
# AuraOS Live Build Monitor — CPU / RAM / per-file compile progress
# Usage: tools/build_monitor.sh [make target...]   (default: all)

set -u
TARGETS=("${@:-all}")
LOG="build/build.log"
mkdir -p build

: > "$LOG"
echo "Starting: make ${TARGETS[*]}"
make -j"$(nproc)" "${TARGETS[@]}" > "$LOG" 2>&1 &
MAKE_PID=$!

tput civis 2>/dev/null || true   # hide cursor
cleanup() { tput cnorm 2>/dev/null; kill "$MAKE_PID" 2>/dev/null; }
trap cleanup INT TERM

while kill -0 "$MAKE_PID" 2>/dev/null; do
    CLEAR=$'\033[H\033[2J'
    # --- System resources -------------------------------------------------
    LOAD=$(cut -d' ' -f1-3 /proc/loadavg)
    MEM=$(free -h --wide | awk '/^Mem:/ {print "used "$3" / "$2"  (free "$4")"}')
    CPU=$(top -bn1 | awk -F',' '/^%Cpu/ {printf "%s", $0; exit}')
    # Active compiler processes
    CC_PROC=$(ps -eo pcpu,pmem,comm,args --sort=-pcpu | grep -E 'gcc|cc1|ld|nasm|grub-mkrescue' | grep -v grep | head -3 | awk '{printf "  %5s%% cpu %5s%% mem  %s %s %s\n",$1,$2,$3,$4,$5}')
    [ -z "$CC_PROC" ] && CC_PROC="  (idle — waiting)"

    # --- Build progress ---------------------------------------------------
    TOTAL=$(grep -cE '\-c |\-o |ld |grub-mkrescue' "$LOG" 2>/dev/null || echo 0)
    LAST=$(grep -E 'gcc|ld |grub-mkrescue|make' "$LOG" 2>/dev/null | tail -4)
    ERRS=$(grep -ciE 'error' "$LOG" 2>/dev/null || echo 0)
    WARNS=$(grep -ciE 'warning' "$LOG" 2>/dev/null || echo 0)

    printf '%s' "$CLEAR"
    cat <<EOF
+--------------------------------------------------------------+
|              AuraOS LIVE BUILD MONITOR                       |
+--------------------------------------------------------------+
| Target : make ${TARGETS[*]}
| Load   : $LOAD
| Memory : $MEM
| $CPU
+--------------------------------------------------------------+
| Active build processes:                                      |
$CC_PROC
+--------------------------------------------------------------+
| Progress: $TOTAL tool invocations | Errors: $ERRS | Warnings: $WARNS
| Latest:                                                       |
$LAST
+--------------------------------------------------------------+
| (Ctrl-C to abort the build)                                  |
+--------------------------------------------------------------+
EOF
    sleep 0.5
done

tput cnorm 2>/dev/null || true
wait "$MAKE_PID"
STATUS=$?
echo
echo "==================== BUILD FINISHED ===================="
if [ $STATUS -eq 0 ]; then
    echo "RESULT: SUCCESS (exit 0)"
    ls -lh build/auraos.elf build/auraos.iso 2>/dev/null | awk '{print "  " $9 "  " $5}'
else
    echo "RESULT: FAILED (exit $STATUS) — last errors:"
    grep -iE 'error' "$LOG" | tail -5
fi
exit $STATUS
