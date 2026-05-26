#!/bin/bash
DIR="$(dirname "$0")"
while true; do
    rlwrap "$DIR/tinylang"
    [ $? -eq 0 ] && break
done
