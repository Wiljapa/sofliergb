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

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>
#include <zmk/battery.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/usb.h>

#define SOFLE_IS_CENTRAL (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

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
#if SOFLE_IS_CENTRAL
    bool usb_output;
    int ble_profile;
    bool connected;
    char layer[5];
    uint8_t mods; /* bits HID: 0 LCTRL 1 LSHIFT 2 LALT 3 LGUI 4 RCTRL 5 RSHIFT 6 RALT 7 RGUI */
#endif
};

static void draw_text(lv_layer_t *layer, const char *txt, int x, int y, int w, lv_color_t color) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = &lv_font_unscii_8;
    d.color = color;
    d.text = txt;
    d.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t a = {.x1 = x, .y1 = y, .x2 = x + w - 1, .y2 = y + 7};
    lv_draw_label(layer, &d, &a);
}

static void __maybe_unused draw_box(lv_layer_t *layer, int x, int y, int w, int h, bool filled) {
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

static void render(struct screen_state s) {
    lv_canvas_fill_bg(portrait_canvas, lv_color_black(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(portrait_canvas, &layer);
    char buf[8];

#if SOFLE_IS_CENTRAL
    /* Saida: USB ou BTn (+ '*' se conectado) */
    if (s.usb_output) {
        snprintf(buf, sizeof(buf), "USB");
    } else {
        snprintf(buf, sizeof(buf), "BT%u%c", (unsigned)((s.ble_profile + 1) % 10), s.connected ? '*' : '-');
    }
    draw_text(&layer, buf, 0, 2, PW, lv_color_white());
#else
    draw_text(&layer, "RIGHT", 0, 2, PW, lv_color_white());
#endif

    /* Bateria (com 'c' se o USB estiver ligado) */
    snprintf(buf, sizeof(buf), "%s%u%%", s.usb_powered ? "+" : "", s.battery);
    draw_text(&layer, buf, 0, 14, PW, lv_color_white());

#if SOFLE_IS_CENTRAL
    /* Linha + camada */
    draw_box(&layer, 0, 28, PW, 1, true);
    draw_text(&layer, s.layer, 0, 34, PW, lv_color_white());
    draw_box(&layer, 0, 46, PW, 1, true);

    /* Modificadores: 4 caixas, cheia = ativo */
    static const struct {
        const char *name;
        uint8_t mask;
    } mods[] = {{"CTL", 0x11}, {"SFT", 0x22}, {"ALT", 0x44}, {"GUI", 0x88}};
    for (int i = 0; i < 4; i++) {
        bool on = (s.mods & mods[i].mask) != 0;
        int y = 54 + i * 18;
        draw_box(&layer, 1, y, PW - 2, 14, on);
        draw_text(&layer, mods[i].name, 1, y + 3, PW - 2,
                  on ? lv_color_black() : lv_color_white());
    }
#endif

    lv_canvas_finish_layer(portrait_canvas, &layer);
    rotate_to_landscape();
    lv_obj_invalidate(landscape_canvas);
}

static struct screen_state get_state(const zmk_event_t *eh) {
    struct screen_state s = {
        .battery = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_powered = zmk_usb_is_powered(),
#endif
    };

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
