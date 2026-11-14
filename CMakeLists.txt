# Modulo do repositorio: tela de status do OLED (LVGL 9) e ajuste dos LEDs na ociosidade
zephyr_library_named(sofle_module)
zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
zephyr_library_include_directories(include)
zephyr_library_sources_ifdef(CONFIG_SOFLE_STATUS_SCREEN src/status_screen.c src/status_events.c)
zephyr_library_sources_ifdef(CONFIG_SOFLE_RGB_IDLE src/rgb_idle.c)

# Tecla que troca o estilo da animacao: so existe no central
if((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library_sources_ifdef(CONFIG_SOFLE_STATUS_SCREEN src/behavior_oled_anim.c)
endif()
