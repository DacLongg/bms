#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
BMS_DIR="$SCRIPT_DIR/BMS"
RUN_BUILD=1
INSTALL_VSCODE_EXT=1

usage() {
    cat <<'USAGE'
Usage: ./setup_env.sh [options]

Options:
  --no-build        Install/check tools only; skip test build.
  --no-vscode-ext  Skip VS Code extension installation.
  -h, --help       Show this help.
USAGE
}

while (($#)); do
    case "$1" in
        --no-build)
            RUN_BUILD=0
            ;;
        --no-vscode-ext)
            INSTALL_VSCODE_EXT=0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if [[ ! -f "$BMS_DIR/Makefile" ]]; then
    printf 'Cannot find %s/Makefile. Run this script from the repository root.\n' "$BMS_DIR" >&2
    exit 1
fi

log() {
    printf '\n==> %s\n' "$*"
}

have_cmd() {
    command -v "$1" >/dev/null 2>&1
}

cmd_path() {
    command -v "$1" 2>/dev/null || true
}

run_as_root() {
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        "$@"
    elif have_cmd sudo; then
        sudo "$@"
    else
        printf 'Missing sudo. Re-run as root or install sudo first.\n' >&2
        exit 1
    fi
}

check_tool() {
    local cmd="$1"
    local label="${2:-$1}"

    if have_cmd "$cmd"; then
        printf '[OK]   %-28s %s\n' "$label" "$(cmd_path "$cmd")"
        return 0
    fi

    printf '[MISS] %s\n' "$label"
    return 1
}

check_optional_tool() {
    local cmd="$1"
    local label="${2:-$1}"

    if have_cmd "$cmd"; then
        printf '[OK]   %-28s %s\n' "$label" "$(cmd_path "$cmd")"
    else
        printf '[SKIP] %-28s optional; OpenOCD flash/debug is supported\n' "$label"
    fi
}

print_status() {
    local failed=0

    log "Checking build/debug tools"
    check_tool make "GNU Make" || failed=1
    check_tool arm-none-eabi-gcc "ARM GCC" || failed=1
    check_tool arm-none-eabi-objcopy "ARM objcopy" || failed=1
    check_tool arm-none-eabi-size "ARM size" || failed=1
    check_tool arm-none-eabi-objdump "ARM objdump" || failed=1
    check_tool openocd "OpenOCD" || failed=1
    check_optional_tool st-flash "ST-Link st-flash"

    if have_cmd arm-none-eabi-gdb; then
        printf '[OK]   %-28s %s\n' "ARM GDB" "$(cmd_path arm-none-eabi-gdb)"
    elif have_cmd gdb-multiarch; then
        printf '[OK]   %-28s %s\n' "GDB multiarch" "$(cmd_path gdb-multiarch)"
    else
        printf '[MISS] ARM GDB or gdb-multiarch\n'
        failed=1
    fi

    return "$failed"
}

install_packages() {
    log "Installing missing packages"

    if have_cmd apt-get; then
        run_as_root apt-get update
        run_as_root apt-get install -y \
            make \
            gcc-arm-none-eabi \
            binutils-arm-none-eabi \
            gdb-multiarch \
            openocd
        run_as_root apt-get install -y stlink-tools || \
            printf 'Warning: could not install optional stlink-tools; OpenOCD flash/debug is still supported.\n' >&2
        return
    fi

    if have_cmd dnf; then
        run_as_root dnf install -y \
            make \
            arm-none-eabi-gcc-cs \
            arm-none-eabi-binutils-cs \
            arm-none-eabi-newlib \
            gdb \
            openocd
        run_as_root dnf install -y stlink || \
            printf 'Warning: could not install optional stlink; OpenOCD flash/debug is still supported.\n' >&2
        return
    fi

    if have_cmd pacman; then
        run_as_root pacman -Sy --needed \
            make \
            arm-none-eabi-gcc \
            arm-none-eabi-binutils \
            arm-none-eabi-newlib \
            arm-none-eabi-gdb \
            openocd
        run_as_root pacman -Sy --needed stlink || \
            printf 'Warning: could not install optional stlink; OpenOCD flash/debug is still supported.\n' >&2
        return
    fi

    cat >&2 <<'MSG'
No supported package manager found.
Install these manually, then re-run this script:
  make
  gcc-arm-none-eabi
  binutils-arm-none-eabi
  gdb-multiarch or arm-none-eabi-gdb
  openocd
  stlink-tools (optional)
MSG
    exit 1
}

ensure_gdb_alias() {
    if have_cmd arm-none-eabi-gdb || ! have_cmd gdb-multiarch; then
        return
    fi

    local user_bin="$HOME/.local/bin"
    local wrapper="$user_bin/arm-none-eabi-gdb"
    mkdir -p "$user_bin"

    if [[ ! -x "$wrapper" ]]; then
        cat > "$wrapper" <<'WRAPPER'
#!/usr/bin/env sh
exec gdb-multiarch "$@"
WRAPPER
        chmod +x "$wrapper"
        printf '[OK]   %-28s %s\n' "Created GDB wrapper" "$wrapper"
    fi

    case ":$PATH:" in
        *":$user_bin:"*) ;;
        *)
            export PATH="$user_bin:$PATH"
            printf 'Added %s to PATH for this setup run.\n' "$user_bin"
            ;;
    esac
}

install_vscode_extensions() {
    if [[ "$INSTALL_VSCODE_EXT" -ne 1 ]]; then
        return
    fi

    log "Checking VS Code extensions"
    if ! have_cmd code; then
        printf "VS Code 'code' command not found; skipping extension installation.\n"
        return
    fi

    code --install-extension marus25.cortex-debug --force >/dev/null || \
        printf 'Warning: failed to install marus25.cortex-debug.\n' >&2
    code --install-extension ms-vscode.cpptools --force >/dev/null || \
        printf 'Warning: failed to install ms-vscode.cpptools.\n' >&2
}

json_escape() {
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

configure_vscode_workspace() {
    log "Configuring VS Code workspace for Linux"

    local vscode_dir="$SCRIPT_DIR/.vscode"
    local gdb_path=""
    local objdump_path=""
    local openocd_path=""
    local gdb_json=""
    local objdump_json=""
    local openocd_json=""

    mkdir -p "$vscode_dir"

    if have_cmd gdb-multiarch; then
        gdb_path="$(cmd_path gdb-multiarch)"
    elif have_cmd arm-none-eabi-gdb; then
        gdb_path="$(cmd_path arm-none-eabi-gdb)"
    else
        gdb_path="/usr/bin/gdb-multiarch"
    fi

    objdump_path="$(cmd_path arm-none-eabi-objdump)"
    openocd_path="$(cmd_path openocd)"

    gdb_json="$(json_escape "${gdb_path:-/usr/bin/gdb-multiarch}")"
    objdump_json="$(json_escape "${objdump_path:-/usr/bin/arm-none-eabi-objdump}")"
    openocd_json="$(json_escape "${openocd_path:-openocd}")"

    cat > "$vscode_dir/launch.json" <<EOF
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug BMS (ST-Link)",
      "cwd": "\${workspaceFolder}/BMS",
      "executable": "\${workspaceFolder}/BMS/build/BMS.elf",
      "request": "launch",
      "type": "cortex-debug",
      "servertype": "openocd",
      "gdbPath": "$gdb_json",
      "objdumpPath": "$objdump_json",
      "configFiles": [
        "interface/stlink.cfg",
        "target/stm32l0.cfg"
      ],
      "device": "STM32L010C6Tx",
      "runToEntryPoint": "main",
      "preLaunchTask": "Rebuild BMS",
      "windows": {
        "gdbPath": "arm-none-eabi-gdb.exe",
        "objdumpPath": "arm-none-eabi-objdump.exe",
        "serverpath": "openocd.exe"
      }
    }
  ]
}
EOF

    cat > "$vscode_dir/tasks.json" <<'EOF'
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Build BMS",
      "type": "shell",
      "command": "make",
      "args": [
        "-C",
        "BMS",
        "all"
      ],
      "windows": {
        "command": "${workspaceFolder}\\build.bat",
        "args": [
          "all"
        ]
      },
      "group": {
        "kind": "build",
        "isDefault": true
      },
      "problemMatcher": [
        "$gcc"
      ]
    },
    {
      "label": "Build BMS UART Protocol",
      "type": "shell",
      "command": "make",
      "args": [
        "-C",
        "BMS",
        "all",
        "USER_DEFS=-DBMS_UART_PROTOCOL_ENABLE=1"
      ],
      "windows": {
        "command": "${workspaceFolder}\\build.bat",
        "args": [
          "all",
          "USER_DEFS=-DBMS_UART_PROTOCOL_ENABLE=1"
        ]
      },
      "problemMatcher": [
        "$gcc"
      ]
    },
    {
      "label": "Rebuild BMS",
      "dependsOrder": "sequence",
      "dependsOn": [
        "Clean BMS",
        "Build BMS"
      ],
      "problemMatcher": []
    },
    {
      "label": "Clean BMS",
      "type": "shell",
      "command": "make",
      "args": [
        "-C",
        "BMS",
        "clean"
      ],
      "windows": {
        "command": "${workspaceFolder}\\build.bat",
        "args": [
          "clean"
        ]
      },
      "problemMatcher": []
    },
    {
      "label": "Flash BMS (ST-Link)",
      "type": "shell",
      "command": "make",
      "args": [
        "-C",
        "BMS",
        "flash-stlink"
      ],
      "windows": {
        "command": "${workspaceFolder}\\build.bat",
        "args": [
          "flash"
        ]
      },
      "problemMatcher": []
    }
  ]
}
EOF

    cat > "$vscode_dir/extensions.json" <<'EOF'
{
  "recommendations": [
    "marus25.cortex-debug",
    "ms-vscode.cpptools"
  ]
}
EOF
}

build_project() {
    if [[ "$RUN_BUILD" -ne 1 ]]; then
        return
    fi

    log "Building BMS firmware"
    make -C "$BMS_DIR" all
}

if ! print_status; then
    install_packages
fi

ensure_gdb_alias

if ! print_status; then
    cat >&2 <<'MSG'
Some required tools are still missing after installation.
Check the messages above, install the missing tool manually, then re-run setup_env.sh.
MSG
    exit 1
fi

configure_vscode_workspace
install_vscode_extensions
build_project

log "Environment setup completed"
printf 'You can build with: make -C BMS all\n'
printf 'You can flash with: make -C BMS flash\n'
printf 'For OpenOCD debug server: make -C BMS debug-server\n'
