# Dog Game

A multiplayer browser game server written in C++20. Players control dogs that run
around a map, collect loot, and deliver it to the base to score points.

The server provides a JSON REST API, serves the static web frontend, simulates the
game world on a tick-based loop, and can persist its state to disk.

## Features

- HTTP server based on Boost.Beast / Boost.Asio, multi-threaded (`io_context`)
- Tick-based game loop (configurable period) with loot spawning, movement and
  collision detection
- Player sessions with token-based authentication
- JSON API for maps, joining, player actions and game state
- State serialization (save/restore) via Boost.Serialization
- Static file serving with MIME support
- Structured logging via Boost.Log
- Unit tests with Catch2
- Docker support

## Requirements

| Tool | Version | Notes |
|------|---------|-------|
| CMake | 3.9+ | |
| Conan | 1.x | CMake generator is used (`conanbuildinfo.cmake`), Conan 2.x will **not** work |
| Compiler | GCC 11+ or Clang 12+ | C++20 required |
| Python 3 | any | Required by Conan |

## Dependencies

Managed via Conan (`conanfile.txt`):

- **Boost 1.86.0** — Asio, Beast, JSON, Serialization, Log, Program Options
- **Catch2 3.1.0** — unit tests
- **pthreads** — linked directly through CMake's `Threads` package

## Build

### Quick build (script)

```bash
./build.sh
```

The script removes the previous `build/` directory, installs dependencies with
Conan and builds the project. Note: it uses `compiler.libcxx=libc++` (macOS /
Clang). On Linux, use `libstdc++11` instead.

### Manual build

```bash
mkdir -p build && cd build
conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Debug --build=missing
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j
```

Replace `libstdc++11` with `libc++` on macOS. A default Conan profile must be
configured (`conan profile detect --force`).

Build artifacts:

- `build/bin/game_server` — the server
- `build/bin/tests` — unit tests

## Run

```bash
./run.sh
```

or directly:

```bash
./build/bin/game_server -c data/config.json -w static -t 100
```

Then open <http://localhost:8080/> in a browser. The server listens on
`0.0.0.0:8080`.

### Command-line options

| Option | Default | Description |
|--------|---------|-------------|
| `-c, --config-file <file>` | required | Path to the game config JSON |
| `-w, --www-root <dir>` | required | Directory with static files to serve |
| `-t, --tick-period <ms>` | `0` | Tick period in milliseconds; `0` disables the ticker and the game time only advances via `POST /api/v1/game/tick` |
| `--randomize-spawn-points` | off | Spawn players at random points on the map |
| `--state-file <file>` | — | File to save/load the server state to/from |
| `--save-state-period <ms>` | `0` | Period of automatic state saving in game-time milliseconds |
| `-h, --help` | | Show help |

## REST API

All endpoints are under `/api/v1`. Everything else is served from the static
files directory. Endpoints marked 🔒 require an
`Authorization: Bearer <authToken>` header, where `authToken` is returned by
`/api/v1/game/join`.

| Method | Endpoint | Auth | Description |
|--------|----------|------|-------------|
| `GET` | `/api/v1/maps` | — | List of maps: `[{"id": "map1", "name": "Map 1"}, ...]` |
| `GET` | `/api/v1/maps/{mapId}` | — | Full map info: roads, buildings, offices, loot types |
| `POST` | `/api/v1/game/join` | — | Join a game. Body: `{"userName": "John", "mapId": "map1"}`. Response: `{"authToken": "...", "playerId": 1}` |
| `GET` | `/api/v1/game/players` | 🔒 | List of players on the map |
| `GET` | `/api/v1/game/state` | 🔒 | Current game state: dogs, scores, lost objects |
| `POST` | `/api/v1/game/player/action` | 🔒 | Move a dog. Body: `{"move": "U"}` — `U`, `D`, `L`, `R`, or `""` to stop |
| `POST` | `/api/v1/game/tick` | — | Advance game time. Body: `{"timeDelta": 100}` (ms). Response: state after the tick |

Example:

```bash
# Join a game
curl -X POST http://localhost:8080/api/v1/game/join \
  -H 'Content-Type: application/json' \
  -d '{"userName": "John", "mapId": "map1"}'

# Move the dog
curl -X POST http://localhost:8080/api/v1/game/player/action \
  -H 'Content-Type: application/json' \
  -H 'Authorization: Bearer <authToken>' \
  -d '{"move": "U"}'
```

## Configuration

The game config is a JSON file (`data/config.json` is an example):

- `defaultDogSpeed` — default dog speed (units/second)
- `defaultBagCapacity` — default bag capacity per dog
- `lootGeneratorConfig` — `period` (base interval, seconds) and `probability`
  of loot spawning per interval
- `maps` — array of maps, each with:
  - `id`, `name`
  - `dogSpeed`, `bagCapacity` (map-level overrides)
  - `lootTypes` — loot items: name, 3D model file, type, rotation, color,
    scale, score value
  - `roads` — horizontal (`x0, y0, x1`) or vertical (`x0, y0, y1`) segments
  - `buildings` — rectangular obstacles
  - `offices` — bases where dogs deliver loot to score

## Tests

```bash
./run_tests.sh
```

Runs the Catch2 binaries covering the loot generator and collision detection.

## Docker

```bash
docker build -t dog-game .
docker run --rm -p 8080:8080 dog-game
```

The image builds the project with GCC 11 in a multi-stage build and runs the
server as an unprivileged `www` user.

## Project structure

```
├── CMakeLists.txt          # build configuration
├── conanfile.txt           # dependency manifest (Conan 1.x)
├── build.sh                # clean build script
├── run.sh                  # run script
├── run_tests.sh            # test runner
├── Dockerfile              # multi-stage Docker build
├── data/
│   └── config.json         # example game configuration
├── src/                    # server sources
│   ├── model.*             # game model: maps, roads, dogs, loot, sessions
│   ├── http_server.*       # Boost.Beast HTTP server
│   ├── api_request_handler.* # REST API
│   ├── static_request_handler.* # static file serving
│   ├── json_loader.*       # config parsing
│   ├── state_serialization.*  # save/load game state
│   ├── loot_generator.*    # probabilistic loot spawning
│   ├── collision_detector.*   # dog-loot collision detection
│   └── ...
├── static/                 # web frontend
└── tests/                  # Catch2 unit tests
```
