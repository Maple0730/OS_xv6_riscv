#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"
cp ../../build/fs.img ./fs.img
echo "Synced ../../build/fs.img -> tools/fsviz/fs.img"
