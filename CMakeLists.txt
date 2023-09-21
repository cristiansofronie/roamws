project(lws-minimal-ws-client-ping C)
cmake_minimum_required(VERSION 2.8.12)
find_package(libwebsockets CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${LWS_CMAKE_DIR})
include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(LwsCheckRequirements)

set(requirements 1)

require_lws_config(LWS_ROLE_WS 1 requirements)
require_lws_config(LWS_WITH_CLIENT 1 requirements)

if (requirements)
  add_executable(ropen ropen.c)

  target_link_libraries(ropen websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
  add_dependencies(ropen websockets_shared)

  add_executable(rsrc rsrc.c)
  target_link_libraries(rsrc websockets_shared ${LIBWEBSOCKETS_DEP_LIBS})
  add_dependencies(rsrc websockets_shared)
endif()
