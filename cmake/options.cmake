option(DEBUG                "Include additional debug-code in core"    OFF)
option(WARNINGS             "Show all warnings during compile"         OFF)
option(PCH                  "Use precompiled headers"                  ON )
option(BUILD_GAME_SERVER    "Build game server"                        ON )
option(BUILD_LOGIN_SERVER   "Build login server"                       OFF)
option(BUILD_EXTRACTORS     "Build map/dbc/vmap/mmap extractors"       OFF)
option(BUILD_SCRIPTDEV      "Build ScriptDev. (OFF Speedup build)"     ON )
option(BUILD_PLAYERBOT      "Build Playerbot mod"                      OFF)
option(BUILD_AHBOT          "Build Auction House Bot mod"              OFF)
option(BUILD_METRICS        "Build Metrics, generate data for Grafana" OFF)

message("")
message(STATUS
  "This script builds the MaNGOS server.
  Options that can be used in order to configure the process:
    CMAKE_INSTALL_PREFIX    Path where the server should be installed to
    PCH                     Use precompiled headers
    DEBUG                   Include additional debug-code in core
    WARNINGS                Show all warnings during compile
    BUILD_GAME_SERVER       Build game server (core server)
    BUILD_LOGIN_SERVER      Build login server (auth server)
    BUILD_EXTRACTORS        Build map/dbc/vmap/mmap extractor
    BUILD_SCRIPTDEV         Build scriptdev. (Disable it to speedup build in dev mode by not including scripts)
    BUILD_PLAYERBOT         Build Playerbot mod
    BUILD_AHBOT             Build Auction House Bot mod
)
message("")
