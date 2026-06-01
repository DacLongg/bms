#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
TARGETS=("$@")

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    TARGETS=(all)
fi

make -C "$SCRIPT_DIR/BMS" "${TARGETS[@]}"
