idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES gb_core gb_video bsp esp_timer mbedtls nvs_flash
)

# The requirements scan returns inside idf_component_register, so the
# ROM path is resolved only in the real configure, where -DGB_ROM is set.
# STRING rather than FILEPATH: FILEPATH rewrites the value against the
# build directory during the early scan.
set(GB_ROM "${CMAKE_SOURCE_DIR}/roms/homebrew/pattern.gb" CACHE STRING
    "DMG ROM embedded in the firmware")
set(GB_RUN_FRAMES "3" CACHE STRING "Emulated frames before gb_hash is printed")
set(GB_OK_JOY "GB_BTN_A" CACHE STRING
    "Joypad mask produced while the OK key is held")
set(GB_INPUT_AT "0" CACHE STRING
    "Frame number that prints gb_input_at before it runs. 0 prints nothing.")

if(NOT EXISTS "${GB_ROM}")
    message(FATAL_ERROR "GB_ROM not found: ${GB_ROM}")
endif()

set(GB_EMBED "${CMAKE_CURRENT_BINARY_DIR}/embedded_rom.gb")
# Copy at build time. A configure-time copy leaves target_add_binary_data
# watching the copy, so editing GB_ROM in place never re-embeds it.
add_custom_command(
    OUTPUT "${GB_EMBED}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${GB_ROM}" "${GB_EMBED}"
    DEPENDS "${GB_ROM}"
    VERBATIM)

target_add_binary_data(${COMPONENT_LIB} "${GB_EMBED}" BINARY
    RENAME_TO embedded_rom
    DEPENDS "${GB_ROM}")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    GB_RUN_FRAMES=${GB_RUN_FRAMES}
    GB_OK_JOY=${GB_OK_JOY}
    GB_INPUT_AT=${GB_INPUT_AT})
