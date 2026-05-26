#!/bin/bash
# Persistent REPL wrapper — restarts on error
DIR="$(cd "$(dirname "$0")" && pwd)"
while true; do
    rlwrap "$DIR/tinylang"
    echo "(restarting...)"
done
