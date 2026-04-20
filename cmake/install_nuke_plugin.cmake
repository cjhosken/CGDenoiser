if(WIN32)
    set(NUKE_DIR "$ENV{USERPROFILE}/.nuke")
else()
    set(NUKE_DIR "$ENV{HOME}/.nuke")
endif()

set(PLUGIN_DIR "${NUKE_DIR}/CGDenoiser")
set(INIT_FILE "${NUKE_DIR}/init.py")

# 1. Copy plugin folder
file(INSTALL
    DESTINATION "${PLUGIN_DIR}"
    TYPE DIRECTORY
    FILES "${CMAKE_SOURCE_DIR}/../plugins/CGDenoiser/"
)

# 2. Ensure init.py exists
if(NOT EXISTS "${INIT_FILE}")
    file(WRITE "${INIT_FILE}" "")
endif()

# 3. Read file safely
file(READ "${INIT_FILE}" INIT_CONTENT)

set(LINE "nuke.pluginAddPath(\"./CGDenoiser\")")

string(FIND "${INIT_CONTENT}" "${LINE}" FOUND)

if(FOUND EQUAL -1)
    file(APPEND "${INIT_FILE}" "\n${LINE}\n")
endif()