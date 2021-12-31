# output generic information about the core
message(STATUS "CMaNGOS-Core revision : ${REVISION_ID}")
message(STATUS "Revision time stamp   : ${REVISION_DATE}")
message(STATUS "Install server to     : ${CMAKE_INSTALL_PREFIX}")

# Show infomation about the options selected during configuration

if(PCH)
  message(STATUS "Use PCH               : Yes (default)")
else()
  message(STATUS "Use PCH               : No")
endif()

if(DEBUG)
  message(STATUS "Build in debug-mode   : Yes")
else()
  message(STATUS "Build in debug-mode   : No  (default)")
endif()

if(BUILD_GAME_SERVER)
  message(STATUS "Build game server     : Yes (default)")
else()
  message(STATUS "Build game server     : No")
endif()

if(BUILD_LOGIN_SERVER)
  message(STATUS "Build login server    : Yes (default)")
else()
  message(STATUS "Build login server    : No")
endif()

if(BUILD_SCRIPTDEV)
  message(STATUS "Build ScriptDev       : Yes")
else()
  message(STATUS "Build ScriptDev       : No")
endif()

if(BUILD_AHBOT)
  message(STATUS "Build AHBot           : Yes")
else()
  message(STATUS "Build AHBot           : No")
endif()

if(BUILD_PLAYERBOT)
  message(STATUS "Build Playerbot       : Yes")
else()
  message(STATUS "Build Playerbot       : No")
endif()

if(BUILD_EXTRACTORS)
  message(STATUS "Build extractors      : Yes")
else()
  message(STATUS "Build extractors      : No")
endif()

if(BUILD_EXTRACTORS)
  message(STATUS "Build map/vmap tools  : Yes")
else()
  message(STATUS "Build map/vmap tools  : No")
endif()

message("")
