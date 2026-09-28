#!/bin/sh
# Fetch the upstream EPD-nRF5 tree (Nordic SDK + tools) this firmware is built on.
set -e
cd "$(dirname "$0")"
REV=7e31961
if [ ! -d upstream/.git ]; then
  git clone https://github.com/tsl0922/EPD-nRF5 upstream
fi
git -C upstream checkout -q "$REV"
