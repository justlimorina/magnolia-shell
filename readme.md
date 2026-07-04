# Magnolia Shell (`mash`)

A lightweight, feature-rich Unix shell written in C++17. Designed to be clean, responsive, and independent of heavy terminal dependency libraries (like readline), using direct POSIX system APIs and terminal raw mode configuration.

## Features

### 1. Interactive Line Editing & Autocomplete
- **Tab Completion**: Auto-complete command names (built-ins & executables in `$PATH`) and file/folder paths. Directory completions automatically append a trailing slash `/`. Supports double-tab listing of matches.
- **Command History**: Navigate through previous commands using the **Up** and **Down** arrow keys. Unsaved current drafts are preserved when navigating.
- **Cursor Control**: Move the cursor back and forth using **Left** and **Right** arrows, or jump to boundaries using **Home** and **End**.
- **Editing Tools**: Modify inputs in place using **Backspace** and **Delete** keys.
- **Ctrl+C / Ctrl+D Support**: Pressing `Ctrl+C` cancels the current input line without terminating the shell. Pressing `Ctrl+D` on an empty line cleanly exits the shell.

### 2. Multi-Stage Pipelines & Redirections
- **Pipelines (`|`)**: Pipe the output of one command to another (e.g., `uptime | grep uptime`).
- **File Redirection**: Redirect standard input using `<` and standard output using `>` (overwrite) or `>>` (append). (e.g., `echo "output" > log.txt`).
- **Wildcard Globbing (`*`)**: Expands file glob patterns automatically (e.g., `ls header/*.h` or `rm *.o`).

### 3. Robust Signal & Terminal Control
- Raw terminal mode is disabled automatically before child processes execute, so full-screen applications (like `nano`, `vim`, or interactive commands like `sudo`) function perfectly.
- Parent process ignores `SIGINT` (Ctrl+C) while waiting for children, allowing child termination without exiting the shell.
- Terminal configurations are safely restored to cooked mode upon exits or abnormal terminations.

### 4. Smart Path & Command Parsing
- **Quote Awareness**: Handles arguments wrapped in single quotes `'...'` or double quotes `"..."` (e.g. `mkdir "my directory"`).
- **Escape Sequences**: Supports backslash escapes `\` for paths with special characters.
- **Tilde Expansion**: Automatically expands `~` to the user's home directory.
- **Environment Variables**: Automatically expands `$VAR` expressions (like `$USER` or `$HOME`) in commands or double quotes.

### 5. Rich Set of Core Built-in Commands
In addition to running external programs via `execvp`, `mash` implements the following built-ins directly:
- **Navigation & Info**: `cd`, `pwd`, `ls` (colorized table-format output), `which`, `uptime`, `free`.
- **File & Directory Ops**: `mkdir`, `rm`, `touch`, `cp`, `mv`, `cat`, `head`, `tail`, `find` (recursive name search).
- **Ownership & Permissions**: `chmod`, `chown`, `chgrp`.
- **System Utilities**: `echo`, `date`, `uname`, `whoami`, `version`, `help`, `history`, `clear`.
- **Environment & Aliases**: `env`, `export`, `alias`, `unalias`, `source` (executes shell script files).

---

## Build Requirements

- A compiler with support for C++17 (e.g., `g++` 8+ or `clang++`).
- A POSIX-compliant environment (Linux or macOS).

---

## Getting Started

### Download Pre-built Binary
You can download the compiled binaries directly from the **Releases** page of this repository:
- **Linux (amd64)**: `mash-linux-amd64`
- **macOS (Universal - Intel & Apple Silicon)**: `mash-darwin-universal`

After downloading, grant execution permissions and run:
```bash
chmod +x mash-linux-amd64 # or mash-darwin-universal
./mash-linux-amd64
```

### Compile from Source
Compile the shell executable using the provided build script:
```bash
./build.sh
```

### Run
Launch the shell:
```bash
./mash
```

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
