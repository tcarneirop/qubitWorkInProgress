
#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QISKIT_DIR="$SCRIPT_DIR/../qiskit"
PATCH_FILE="$SCRIPT_DIR/qiskit-2.6.2-sabre-routing.patch"

QISKIT_VERSION="2.6.2"

echo "=== Qiskit C Setup ==="

# Load Rust environment if available.
if [[ -f "$HOME/.cargo/env" ]]; then
    source "$HOME/.cargo/env"
fi

# Check required tools.
for tool in git cargo rustc make g++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: Missing dependency: $tool"
        exit 1
    fi
done

# Clone Qiskit if necessary.
if [[ ! -d "$QISKIT_DIR" ]]; then
    echo "Cloning Qiskit $QISKIT_VERSION..."

    git clone --branch "$QISKIT_VERSION" --depth 1 \
        https://github.com/Qiskit/qiskit.git "$QISKIT_DIR"
fi

# Verify repository and version.
if [[ ! -d "$QISKIT_DIR/.git" ]]; then
    echo "ERROR: $QISKIT_DIR is not a Git repository"
    exit 1
fi

VERSION="$(git -C "$QISKIT_DIR" describe --tags --exact-match)"

if [[ "$VERSION" != "$QISKIT_VERSION" ]]; then
    echo "ERROR: Expected Qiskit $QISKIT_VERSION, found $VERSION"
    exit 1
fi

# Verify patch exists.
if [[ ! -f "$PATCH_FILE" ]]; then
    echo "ERROR: Patch not found: $PATCH_FILE"
    exit 1
fi

# Apply patch only when necessary.
if git -C "$QISKIT_DIR" apply --reverse --check "$PATCH_FILE" 2>/dev/null; then
    echo "Patch already applied."
elif git -C "$QISKIT_DIR" apply --check "$PATCH_FILE"; then
    echo "Applying SABRE Routing patch..."
    git -C "$QISKIT_DIR" apply "$PATCH_FILE"
else
    echo "ERROR: Patch cannot be applied cleanly."
    exit 1
fi

# Build Qiskit C API.
echo "Building Qiskit C API..."

make -C "$QISKIT_DIR" c

# Validate library and patched header.
LIBRARY="$QISKIT_DIR/dist/c/lib/libqiskit.so"
HEADER="$QISKIT_DIR/dist/c/include/qiskit/types.h"

if [[ ! -f "$LIBRARY" ]]; then
    echo "ERROR: libqiskit.so was not generated"
    exit 1
fi

if ! grep -Eq 'size_t[[:space:]]+num_trials;' "$HEADER"; then
    echo "ERROR: num_trials not found in Qiskit C header"
    exit 1
fi

echo "Qiskit C library and patch verified."

# Build and execute project tests.
echo "Testing qiskit_c..."

make -C "$SCRIPT_DIR" target
make -C "$SCRIPT_DIR" mapping

echo
echo "=== Setup completed successfully ==="
