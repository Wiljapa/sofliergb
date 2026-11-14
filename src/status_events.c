/*
 * Repasse de camada e modificadores do central para o periferico.
 * Central: escuta camada/teclas, levanta sofle_status_changed e o relay envia ao direito.
 * Periferico: recebe pelo relay e levanta o mesmo evento localmente (a tela escuta).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <string.h>
#include <zmk/event_manager.h>
#include <sofle_status.h>

ZMK_EVENT_IMPL(sofle_status_changed);

#if IS_ENABLED(CONFIG_ZMK_SPLIT_RELAY_EVENT)

#if SOFLE_IS_CENTRAL

#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>

static uint8_t last_layer = 0xFF;
static uint8_t last_mods = 0xFF;

static void publish(const zmk_event_t *eh, bool force) {
    uint8_t layer = (uint8_t)zmk_keymap_highest_layer_active();
    uint8_t mods = zmk_hid_get_keyboard_report()->body.modifiers;

    /* O HID pode ainda nao ter processado este evento: aplica a mudanca na mao. */
    const struct zmk_keycode_state_changed *kev = as_zmk_keycode_state_changed(eh);
    if (kev != NULL && kev->usage_page == HID_USAGE_KEY && kev->keycode >= 0xE0 &&
        kev->keycode <= 0xE7) {
        uint8_t bit = BIT(kev->keycode - 0xE0);
        mods = kev->state ? (mods | bit) : (mods & ~bit);
    }

    if (!force && layer == last_layer && mods == last_mods) {
        return;
    }
    last_layer = layer;
    last_mods = mods;

    raise_sofle_status_changed((struct sofle_status_changed){
        .layer = layer,
        .mods = mods,
        .source = ZMK_RELAY_EVENT_SOURCE_SELF,
    });
}

static int status_tx_cb(const zmk_event_t *eh) {
    bool force = false;
    const struct zmk_position_state_changed *pev = as_zmk_position_state_changed(eh);
    if (pev != NULL) {
        /* Tecla do lado direito apertada: reenvia o estado (o direito pode ter acordado
         * ou reconectado e nao saber a camada). Teclas locais e solturas sao ignoradas. */
        if (pev->source == ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL || !pev->state) {
            return ZMK_EV_EVENT_BUBBLE;
        }
        force = true;
    }
    publish(eh, force);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(sofle_status_tx, status_tx_cb);
ZMK_SUBSCRIPTION(sofle_status_tx, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(sofle_status_tx, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(sofle_status_tx, zmk_position_state_changed);

/* "sst" cabe no limite de 4 bytes do nome do evento de relay (com o terminador). */
ZMK_RELAY_EVENT_CENTRAL_TO_PERIPHERAL(sofle_status_changed, sst, source);

#else /* periferico */

ZMK_RELAY_EVENT_HANDLE(sofle_status_changed, sst, source);

#endif /* SOFLE_IS_CENTRAL */

#endif /* CONFIG_ZMK_SPLIT_RELAY_EVENT */
