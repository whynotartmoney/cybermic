idf_component_register(
    SRCS "main.c" "mick_field.c" "mick_ui.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer freertos log
)

target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/mick_latin_16.c"
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/mick_latin_28.c"
)
