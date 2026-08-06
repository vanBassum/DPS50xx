# ──────────────────────────────────────────────────────────────
# Board fragment: DPS50xx controller on an ESP32 DevKit
#   Generic ESP32-WROOM-32 devkit (4 MB flash) · LED on GPIO2
#   · Modbus RTU to the supply on UART1 (TX 17 / RX 16)
#
# A board fragment may append to BOARD_SOURCES (extra .cpp files under this
# folder that need compiling). Component deps are NOT set here — see the note
# in main/CMakeLists.txt: managed deps go in main/idf_component.yml, IDF
# built-ins in COMPONENT_REQUIRES.
# ──────────────────────────────────────────────────────────────

list(APPEND BOARD_SOURCES "${CMAKE_CURRENT_LIST_DIR}/BoardContext.cpp")
