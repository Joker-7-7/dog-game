#!/usr/bin/env bash
set -e

build/bin/game_server -c data/config.json -w static -t 100
