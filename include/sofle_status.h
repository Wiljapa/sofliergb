/*
 * Evento proprio que leva a camada ativa e os modificadores do central (metade esquerda)
 * ate o periferico (metade direita), para o OLED dele mostrar o mesmo que o do esquerdo.
 * Usa o mecanismo de relay de eventos do fork ZMK main+dya (CONFIG_ZMK_SPLIT_RELAY_EVENT).
 */
#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>

#define SOFLE_IS_CENTRAL (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

struct sofle_status_changed {
    uint8_t layer; /* indice da camada mais alta ativa */
    uint8_t mods;  /* bits HID: 0 LCTRL 1 LSHIFT 2 LALT 3 LGUI 4 RCTRL 5 RSHIFT 6 RALT 7 RGUI */
    uint8_t source; /* ZMK_RELAY_EVENT_SOURCE_SELF (0xFF) se nasceu aqui */
};

ZMK_EVENT_DECLARE(sofle_status_changed);
