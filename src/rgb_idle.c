/*
 * LEDs na ociosidade: apaga ao ficar parado e RESTAURA ao voltar, inclusive depois do sono
 * profundo (que reinicia o teclado).
 *
 * Por que nao usar CONFIG_ZMK_RGB_UNDERGLOW_AUTO_OFF_IDLE: o ZMK grava "LEDs desligados" na
 * memoria ao apagar por ociosidade. Depois do sono profundo o teclado reinicia, le "desligado"
 * e os LEDs ficam apagados ate o usuario ligar. Aqui guardamos separadamente se o apagao foi
 * automatico (ocioso) ou do usuario, e so restauramos o automatico.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/activity.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/rgb_underglow.h>

/* Verdadeiro quando os LEDs foram apagados POR OCIOSIDADE (e nao pelo usuario). */
static bool auto_off;

static void save_flag(bool v) {
    uint8_t b = v ? 1 : 0;
    int rc = settings_save_one("sofle/rgb_auto_off", &b, sizeof(b));
    if (rc != 0) {
        LOG_WRN("rgb_idle: nao consegui gravar a marca (%d)", rc);
    }
}

static void resume(void) {
    if (!auto_off) {
        return;
    }
    auto_off = false;
    save_flag(false);
    zmk_rgb_underglow_on();
}

static int rgb_idle_cb(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->state == ZMK_ACTIVITY_ACTIVE) {
        resume();
    } else if (!auto_off) { /* IDLE ou SLEEP */
        bool on = false;
        if (zmk_rgb_underglow_get_state(&on) == 0 && on) {
            auto_off = true;
            save_flag(true); /* grava ja: o sono profundo pode vir logo depois */
            zmk_rgb_underglow_off();
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(sofle_rgb_idle, rgb_idle_cb);
ZMK_SUBSCRIPTION(sofle_rgb_idle, zmk_activity_state_changed);

/* No boot: se a marca estava gravada (acordou do sono profundo), religa os LEDs. */
static void boot_resume_cb(struct k_work *work) {
    resume();
}
static K_WORK_DELAYABLE_DEFINE(boot_resume_work, boot_resume_cb);

static int sofle_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                              void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "rgb_auto_off", &next) && !next) {
        uint8_t b = 0;
        if (len == sizeof(b) && read_cb(cb_arg, &b, sizeof(b)) >= 0) {
            auto_off = (b != 0);
        }
    }
    return 0;
}

static int sofle_settings_commit(void) {
    if (auto_off) {
        /* espera o driver de LEDs terminar de iniciar */
        k_work_schedule(&boot_resume_work, K_SECONDS(2));
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(sofle_rgb, "sofle", NULL, sofle_settings_set,
                               sofle_settings_commit, NULL);
