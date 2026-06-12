#!/bin/bash

REPO="$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null)"

if [ -z "$REPO" ]; then
    echo "ERROR: not inside a git repo or gh not logged in"
    exit 1
fi

echo "[+] Purging old libs..."
rm -rf "libs/arm64-v8a"
mkdir -p "libs/arm64-v8a"

echo "[+] Downloading menu binary from $REPO..."
gh run download \
    --repo "$REPO" \
    --name "menu-binary" \
    --dir "libs/arm64-v8a"

if [ $? -ne 0 ]; then
    echo "[!] ERROR: menu download failed — make sure the build Action completed"
    exit 1
fi

echo "[+] Done!"
echo "    libmenu.so -> libs/arm64-v8a/libmenu.so"
