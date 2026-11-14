/*
 * Tela de status propria para o OLED 128x32 do Sofle, montado EM PE na placa.
 * Desenha num canvas retrato (32x128) e gira 90 graus para a tela paisagem.
 * Mostra: saida (USB/BT), bateria, camada e modificadores (CTL/SFT/ALT/GUI).
 * Escrita para LVGL 9 / ZMK com Zephyr 4.1 (o zmk-nice-oled e LVGL 8).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <lvgl.h>
#include "draw/lv_draw_triangle.h"

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/usb.h>

#include <sofle_status.h>

#if !SOFLE_IS_CENTRAL
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#endif

#if SOFLE_IS_CENTRAL
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#endif

/* Tela fisica: 128 x 32. Canvas de desenho (retrato): 32 x 128. */
#define PW 32
#define PH 128
#define LW PH
#define LH PW

/* Buffers em L8 (tons de cinza); o driver converte para 1 bit. */
static uint8_t portrait_buf[PW * PH] __aligned(4);
static uint8_t landscape_buf[LW * LH] __aligned(4);

static lv_obj_t *portrait_canvas; /* nao exibido: so para desenhar */
static lv_obj_t *landscape_canvas;

struct screen_state {
    uint8_t battery;
    bool usb_powered;
    uint8_t mods; /* bits HID: 0 LCTRL 1 LSHIFT 2 LALT 3 LGUI 4 RCTRL 5 RSHIFT 6 RALT 7 RGUI */
#if !SOFLE_IS_CENTRAL
    bool link; /* ligado ao esquerdo */
#endif
#if SOFLE_IS_CENTRAL
    bool usb_output;
    int ble_profile;
    bool connected;
#endif
    char layer[5];
};

static void draw_text(lv_layer_t *layer, const char *txt, int x, int y, int w, lv_color_t color) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = &lv_font_unscii_8;
    d.color = color;
    d.text = txt;
    d.text_local = 1; /* o LVGL copia a string: o desenho so acontece no finish_layer */
    d.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t a = {.x1 = x, .y1 = y, .x2 = x + w - 1, .y2 = y + 7};
    lv_draw_label(layer, &d, &a);
}

static void draw_box(lv_layer_t *layer, int x, int y, int w, int h, bool filled) {
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.radius = 2;
    r.bg_color = lv_color_white();
    r.bg_opa = filled ? LV_OPA_COVER : LV_OPA_TRANSP;
    r.border_color = lv_color_white();
    r.border_width = 1;
    r.border_opa = LV_OPA_COVER;
    lv_area_t a = {.x1 = x, .y1 = y, .x2 = x + w - 1, .y2 = y + h - 1};
    lv_draw_rect(layer, &r, &a);
}

static void rotate_to_landscape(void) {
    const uint32_t pstride = lv_draw_buf_width_to_stride(PW, LV_COLOR_FORMAT_L8);
    const uint32_t lstride = lv_draw_buf_width_to_stride(LW, LV_COLOR_FORMAT_L8);
    for (int sy = 0; sy < PH; sy++) {
        for (int sx = 0; sx < PW; sx++) {
            uint8_t v = portrait_buf[sy * pstride + sx];
#if IS_ENABLED(CONFIG_SOFLE_STATUS_SCREEN_ROTATE_CW)
            int dx = (PH - 1) - sy;
            int dy = sx;
#else
            int dx = sy;
            int dy = (PW - 1) - sx;
#endif
            landscape_buf[dy * lstride + dx] = v;
        }
    }
}

static void draw_line(lv_layer_t *layer, int x1, int y1, int x2, int y2) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = x1;
    d.p1.y = y1;
    d.p2.x = x2;
    d.p2.y = y2;
    d.color = lv_color_white();
    d.width = 1;
    d.opa = LV_OPA_COVER;
    lv_draw_line(layer, &d);
}

static void draw_tri(lv_layer_t *layer, int x1, int y1, int x2, int y2, int x3, int y3) {
    lv_draw_triangle_dsc_t t;
    lv_draw_triangle_dsc_init(&t);
    t.p[0].x = x1;
    t.p[0].y = y1;
    t.p[1].x = x2;
    t.p[1].y = y2;
    t.p[2].x = x3;
    t.p[2].y = y3;
    t.bg_color = lv_color_white();
    t.bg_opa = LV_OPA_COVER;
    lv_draw_triangle(layer, &t);
}

/* Runa do Bluetooth, caixa de 9 x 15 px. */
static void draw_bt_icon(lv_layer_t *layer, int x, int y) {
    draw_line(layer, x + 0, y + 4, x + 8, y + 10);
    draw_line(layer, x + 8, y + 10, x + 4, y + 14);
    draw_line(layer, x + 4, y + 14, x + 4, y + 0);
    draw_line(layer, x + 4, y + 0, x + 8, y + 4);
    draw_line(layer, x + 8, y + 4, x + 0, y + 10);
}

/* Raio (carregando), caixa de 8 x 15 px. */
static void draw_bolt_icon(lv_layer_t *layer, int x, int y) {
    draw_tri(layer, x + 6, y + 0, x + 1, y + 8, x + 5, y + 8);
    draw_tri(layer, x + 3, y + 7, x + 7, y + 7, x + 2, y + 15);
}

/* Bateria 26 x 11 px com o nivel preenchido. */
static void draw_battery_icon(lv_layer_t *layer, int x, int y, uint8_t level) {
    draw_box(layer, x, y, 24, 11, false);
    draw_box(layer, x + 24, y + 3, 2, 5, true); /* ponta */
    int fill = (20 * (level > 100 ? 100 : level)) / 100;
    if (fill > 0) {
        draw_box(layer, x + 2, y + 2, fill, 7, true);
    }
}

static void draw_mods(lv_layer_t *layer, uint8_t mask_state, int y0, int step, int h) {
    static const struct {
        const char *name;
        uint8_t mask;
    } mods[] = {{"CTL", 0x11}, {"SFT", 0x22}, {"ALT", 0x44}, {"GUI", 0x88}};
    for (int i = 0; i < 4; i++) {
        bool on = (mask_state & mods[i].mask) != 0;
        int y = y0 + i * step;
        draw_box(layer, 1, y, PW - 2, h, on);
        draw_text(layer, mods[i].name, 1, y + (h - 8) / 2, PW - 2,
                  on ? lv_color_black() : lv_color_white());
    }
}

static void render(struct screen_state s) {
    lv_canvas_fill_bg(portrait_canvas, lv_color_black(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(portrait_canvas, &layer);
    char buf[8];

    /* Linha 1: conexao */
#if SOFLE_IS_CENTRAL
    if (s.usb_output) {
        draw_text(&layer, "USB", 0, 5, 24, lv_color_white());
    } else {
        draw_bt_icon(&layer, 1, 1);
        snprintf(buf, sizeof(buf), "%u%c", (unsigned)((s.ble_profile + 1) % 10),
                 s.connected ? '*' : '-');
        draw_text(&layer, buf, 11, 5, 16, lv_color_white());
    }
#else
    draw_bt_icon(&layer, 1, 1);
    draw_text(&layer, s.link ? "OK" : "--", 11, 5, 16, lv_color_white());
#endif
    if (s.usb_powered) {
        draw_bolt_icon(&layer, 23, 1);
    }

    /* Linha 2: bateria (icone + porcentagem) */
    draw_battery_icon(&layer, 2, 19, s.battery);
    snprintf(buf, sizeof(buf), "%u%%", s.battery);
    draw_text(&layer, buf, 0, 33, PW, lv_color_white());
    draw_box(&layer, 0, 44, PW, 1, true);

    draw_text(&layer, s.layer, 0, 49, PW, lv_color_white());
    draw_box(&layer, 0, 60, PW, 1, true);
    draw_mods(&layer, s.mods, 65, 16, 13);

    lv_canvas_finish_layer(portrait_canvas, &layer);
    rotate_to_landscape();
    lv_obj_invalidate(landscape_canvas);
}

#if !SOFLE_IS_CENTRAL
/* Posicoes das teclas modificadoras do lado direito no keymap do Sofle (60 posicoes:
 * 0-11, 12-23, 24-35, 36-49 (inclui os 2 botoes de dial), 50-59 polegares). */
static uint8_t periph_mods;
static uint8_t remote_layer = 0xFF; /* vem do central pelo relay; 0xFF = ainda desconhecida */
static uint8_t remote_mods;

static void layer_label(char *out, uint8_t idx) {
    static const char *const names[] = {"BASE", "LOWE", "RAIS", "ADJU"};
    if (idx < ARRAY_SIZE(names)) {
        strcpy(out, names[idx]);
    } else {
        snprintf(out, 5, "L%u", idx % 100);
    }
}

static void periph_track(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *pev = as_zmk_position_state_changed(eh);
    if (pev == NULL) {
        return;
    }
    uint8_t bit = 0;
    switch (pev->position) {
    case 49: bit = 0x20; break; /* RSHIFT */
    case 57: bit = 0x10; break; /* RCTRL  */
    case 58: bit = 0x40; break; /* RALT   */
    case 59: bit = 0x80; break; /* RGUI   */
    default: return;
    }
    periph_mods = pev->state ? (periph_mods | bit) : (periph_mods & ~bit);
}
#endif

static struct screen_state get_state(const zmk_event_t *eh) {
    struct screen_state s = {
        .battery = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_powered = zmk_usb_is_powered(),
#endif
    };

#if !SOFLE_IS_CENTRAL
    periph_track(eh);
    const struct sofle_status_changed *sev = as_sofle_status_changed(eh);
    if (sev != NULL && sev->source != ZMK_RELAY_EVENT_SOURCE_SELF) {
        remote_layer = sev->layer;
        remote_mods = sev->mods;
    }
    s.mods = periph_mods | remote_mods;
    s.link = zmk_split_bt_peripheral_is_connected();
    if (remote_layer == 0xFF) {
        strcpy(s.layer, "----");
    } else {
        layer_label(s.layer, remote_layer);
    }
#endif

#if SOFLE_IS_CENTRAL
    struct zmk_endpoint_instance ep = zmk_endpoint_get_selected();
    s.usb_output = (ep.transport == ZMK_TRANSPORT_USB);
    s.ble_profile = zmk_ble_active_profile_index();
    s.connected = zmk_endpoint_is_connected();

    zmk_keymap_layer_index_t idx = zmk_keymap_highest_layer_active();
    const char *name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(idx));
    if (name == NULL || name[0] == '\0') {
        snprintf(s.layer, sizeof(s.layer), "L%d", idx);
    } else {
        for (int i = 0; i < 4 && name[i]; i++) {
            s.layer[i] = toupper((unsigned char)name[i]);
        }
    }

    s.mods = zmk_hid_get_keyboard_report()->body.modifiers;
    /* O HID pode ainda nao ter processado este evento: aplica a mudanca na mao. */
    const struct zmk_keycode_state_changed *kev = as_zmk_keycode_state_changed(eh);
    if (kev != NULL && kev->usage_page == HID_USAGE_KEY && kev->keycode >= 0xE0 &&
        kev->keycode <= 0xE7) {
        uint8_t bit = BIT(kev->keycode - 0xE0);
        s.mods = kev->state ? (s.mods | bit) : (s.mods & ~bit);
    }
#endif
    return s;
}

static void update_cb(struct screen_state s) {
    if (landscape_canvas != NULL) {
        render(s);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(sofle_screen, struct screen_state, update_cb, get_state)

ZMK_SUBSCRIPTION(sofle_screen, zmk_battery_state_changed);
#if !SOFLE_IS_CENTRAL
ZMK_SUBSCRIPTION(sofle_screen, zmk_position_state_changed);
ZMK_SUBSCRIPTION(sofle_screen, zmk_split_peripheral_status_changed);
ZMK_SUBSCRIPTION(sofle_screen, sofle_status_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(sofle_screen, zmk_usb_conn_state_changed);
#endif
#endif
#if SOFLE_IS_CENTRAL
ZMK_SUBSCRIPTION(sofle_screen, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(sofle_screen, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(sofle_screen, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(sofle_screen, zmk_ble_active_profile_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(sofle_screen, zmk_usb_conn_state_changed);
#endif
#endif

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    /* Canvas retrato: fora da tela (nunca desenhado), so serve de area de desenho. */
    portrait_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(portrait_canvas, portrait_buf, PW, PH, LV_COLOR_FORMAT_L8);
    lv_obj_add_flag(portrait_canvas, LV_OBJ_FLAG_HIDDEN);

    landscape_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(landscape_canvas, landscape_buf, LW, LH, LV_COLOR_FORMAT_L8);
    lv_obj_align(landscape_canvas, LV_ALIGN_TOP_LEFT, 0, 0);

    sofle_screen_init();
    return screen;
}
