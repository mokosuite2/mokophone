
#include "globals.h"
#include "gsm.h"

#include <mokosuite/ui/gui.h>
#include <mokosuite/utils/misc.h>
#include <mokosuite/utils/utils.h>
#include <mokosuite/utils/notify.h>
#include <mokosuite/utils/remote-config-service.h>
#include <freesmartphone-glib/freesmartphone-glib.h>
#include <freesmartphone-glib/ogsmd/call.h>
#include <freesmartphone-glib/ousaged/usage.h>
#include <freesmartphone-glib/odeviced/idlenotifier.h>
#include <freesmartphone-glib/odeviced/vibrator.h>
#include <freesmartphone-glib/odeviced/audio.h>
#include <freesmartphone-glib/odeviced/led.h>

#include "callwin.h"
#include "callblock.h"
#include "sound.h"

#define CALL_VIBRATE_PULSES             200
#define CALL_VIBRATE_ON_DURATION        600
#define CALL_VIBRATE_OFF_DURATION       300
#define CALL_VIBRATE_STRENGTH           100

// suoneria predefinita
#define CALL_DEFAULT_RINGTONE           "/usr/share/sounds/ringtone_ringnroll.wav"

#define CALL_LED_PATH                   "/org/freesmartphone/Device/LED/gta02_red_aux"

/* finestra principale */
static MokoWin* win = NULL;

/* lista blocchi chiamate */
static Eina_List* calls = NULL;

/* pulsanti menu hover */
static Evas_Object *bt_keypad;
static Evas_Object *bt_closeall;
static Evas_Object *bt_mute;
static Evas_Object *bt_speaker;

// impostazioni notifiche
static gboolean call_notification_sound = FALSE;
static gboolean call_notification_vibration = FALSE;
static char* call_notification_ringtone = NULL;

static char* call_notification_current_ringtone = NULL;

static int current_mute_status = 0;

// ongoing call notification
NotifyNotification* call_notification = NULL;


/**
 * Fa partire una notifica di chiamata.
 * @param in_call flag che indica se abbiamo gia' chiamate attive
 */
static void call_notification_start(gboolean in_call)
{
    if (in_call) {
        // TODO notifica in chiamata attiva

    } else {
        // notifica senza chiamata attiva

        // vibrazione
        if (call_notification_vibration && odevicedVibratorBus) {
            odeviced_vibrator_vibrate_pattern(CALL_VIBRATE_PULSES, CALL_VIBRATE_ON_DURATION, CALL_VIBRATE_OFF_DURATION,
                CALL_VIBRATE_STRENGTH, NULL, NULL);
        }

        // suoneria
        g_debug("NOTIFICATION: sound=%d, ringtone=%s", call_notification_sound, call_notification_ringtone);
        if (call_notification_sound && call_notification_ringtone) {

            // non si sa mai...
            if (call_notification_current_ringtone)
                g_free(call_notification_current_ringtone);

            call_notification_current_ringtone = g_strdup(call_notification_ringtone);

            odeviced_audio_play_sound(call_notification_current_ringtone, 1, 0, NULL, NULL);
        }
    }

    // blinka aux led
    odeviced_led_set_blinking(CALL_LED_PATH, 100, 1000, NULL, NULL);
}

/**
 * Ferma una notifica di chiamata.
 */
static void call_notification_stop()
{
    // ferma vibrazione
    if (odevicedVibratorBus)
        odeviced_vibrator_stop(NULL, NULL);

    // ferma suoneria
    if (call_notification_current_ringtone) {
        odeviced_audio_stop_sound(call_notification_current_ringtone, NULL, NULL);

        // cancella suoneria corrente
        g_free(call_notification_current_ringtone);
        call_notification_current_ringtone = NULL;
    }

    // ferma aux led
    odeviced_led_set_brightness(CALL_LED_PATH, 0, NULL, NULL);
}

static void call_notification_settings(RemoteConfigService *object, const char* section, const char *key, GValue *value)
{
    if (!strcmp(section, "phone")) {
        if (!strcmp(key, CALL_NOTIFICATION_SOUND)) {
            call_notification_sound = g_value_get_boolean(value);
        }

        else if (!strcmp(key, CALL_NOTIFICATION_VIBRATION)) {
            call_notification_vibration = g_value_get_boolean(value);
        }

        else if (!strcmp(key, CALL_NOTIFICATION_RINGTONE)) {
            if (call_notification_ringtone != NULL)
                g_free(call_notification_ringtone);

            call_notification_ringtone = g_strdup(g_value_get_string(value));
        }
    }
}

// callback presenza cuffie
static gboolean headset_presence(gpointer data)
{
    gboolean presence = GPOINTER_TO_INT(data);
    if (presence)
        sound_state_set(SOUND_STATE_HEADSET);
    else
        sound_state_set(SOUND_STATE_HANDSET);

    sound_volume_mute_set(CONTROL_MICROPHONE, current_mute_status);
    elm_button_label_set(bt_speaker, _("Speaker"));

    // non serve...
    return FALSE;
}

static void _delete(void* mokowin, Evas_Object* obj, void* event_info)
{
    mokowin_hide((MokoWin *)mokowin);
}

static void _notify_activate(NotifyNotification* notify, char* action, void* data)
{
    // show call win
    phone_call_win_activate();
}

static void sync_call_notification(void)
{
    char* msg;
    char* body;
    int count = eina_list_count(calls);

    // no calls - remove if present
    if (!count) {
        if (call_notification) {
            notify_notification_close(call_notification, NULL);
            g_object_unref(call_notification);
            call_notification = NULL;
        }
        return;
    }

    // one call - use peer
    else if (count == 1) {
        PhoneCallBlock* c = calls->data;
        msg = g_strdup(c->peer);
        // TODO body: timer if single call
        body = NULL;
    }
    else {
        msg = g_strdup_printf(_("%d active calls"), count);
    }

    if (!call_notification) {
        call_notification = mokosuite_notification_new(
            "phone.call.active",    // category
            msg,                    // summary
            body,                   // body
            NULL,                   // icon TODO
            NOTIFICATION_HINT_DONT_PUSH     // don't push to panel top
            |
            NOTIFICATION_HINT_ONGOING       // ongoing notification
        );
        notify_notification_set_timeout(call_notification, 0);
        notify_notification_set_hint_string(call_notification, "image_path", "file://" MOKOPHONE_DATADIR "/call-start.png");
        notify_notification_add_action(call_notification, "activate", _("Show"), NOTIFY_ACTION_CALLBACK(_notify_activate), NULL, NULL);
    }
    else
        notify_notification_update(call_notification, msg, body, NULL);

    notify_notification_show(call_notification, NULL);
}

static PhoneCallBlock* append_callblock(const char *peer, int id, gboolean outgoing)
{

    PhoneCallBlock *b = phone_call_block_new(win, peer, id, outgoing);

    // aggiungi la chiamata alla lista e alla vbox
    calls = eina_list_append(calls, b);

    // ottieni la call block precedente
    int c = eina_list_count(calls);
    if (c > 1) {
        PhoneCallBlock *prev = (PhoneCallBlock *) eina_list_nth(calls, c - 2);
        elm_box_pack_after(win->vbox, b->widget, prev->widget);
    } else {
        elm_box_pack_start(win->vbox, b->widget);

        // prima chiamata aggiunta, occupa CPU
        ousaged_usage_request_resource("CPU", NULL, NULL);
    }

    // update notification
    sync_call_notification();
    return b;
}

static void hold_all(void)
{
    PhoneCallBlock *c = NULL;
    Eina_List* iter;
    gboolean found = FALSE;

    EINA_LIST_FOREACH(calls, iter, c) {
        // chiamata in attesa -- ok
        if (c->status == CALL_STATUS_HELD) continue;

        // chiamata attiva -- metti in attesa tutto
        if (c->status == CALL_STATUS_ACTIVE) {

            c->status = CALL_STATUS_HELD;
            phone_call_block_update_hold(c);

            found = TRUE;
        }
    }

    if (found)
        ogsmd_call_hold_active(NULL, NULL);

}

static void release_all(void)
{
    ogsmd_call_release_all(NULL, NULL);
}

static void _release_all(void* data, Evas_Object* obj, void* event_info)
{
    mokowin_menu_hide(win);
    release_all();
}

static void _switch_mute(void* data, Evas_Object* obj, void* event_info)
{
    int cur = sound_volume_mute_get(CONTROL_MICROPHONE);
    g_debug("Current mute: %d", cur);

    // errore :(
    if (cur < 0) return;

    if (!cur) {

        sound_volume_mute_set(CONTROL_MICROPHONE, 1);
        elm_button_label_set(obj, _("Unmute"));

    } else {

        sound_volume_mute_set(CONTROL_MICROPHONE, 0);
        elm_button_label_set(obj, _("Mute"));

    }
}

static void _switch_speaker(void* data, Evas_Object* obj, void* event_info)
{
    enum SoundState cur = sound_state_get();

    // eravamo con l'altoparlante interno, cambia a speaker
    if (cur == SOUND_STATE_HANDSET || cur == SOUND_STATE_HEADSET) {

        sound_state_set(SOUND_STATE_SPEAKER);
        sound_volume_mute_set(CONTROL_MICROPHONE, current_mute_status);
        elm_button_label_set(obj, _("Handset"));

    } else if (cur == SOUND_STATE_SPEAKER) {

        if (sound_headset_present())
            sound_state_set(SOUND_STATE_HEADSET);
        else
            sound_state_set(SOUND_STATE_HANDSET);

        sound_volume_mute_set(CONTROL_MICROPHONE, current_mute_status);
        elm_button_label_set(obj, _("Speaker"));

    }
}

static Evas_Object* make_menu(void)
{
    Evas_Object *m = elm_table_add(win->win);
    elm_table_homogenous_set(m, TRUE);

    evas_object_size_hint_weight_set(m, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(m, EVAS_HINT_FILL, 1.0);

    bt_keypad = mokowin_menu_hover_button(win, m, _("Keypad"), 0, 0, 3, 1);

    bt_closeall = mokowin_menu_hover_button(win, m, _("Close all"), 0, 1, 1, 1);
    evas_object_smart_callback_add(bt_closeall, "clicked", _release_all, NULL);

    bt_mute = mokowin_menu_hover_button(win, m, _("Mute"), 1, 1, 1, 1);
    evas_object_smart_callback_add(bt_mute, "clicked", _switch_mute, NULL);

    bt_speaker = mokowin_menu_hover_button(win, m, _("Speaker"), 2, 1, 1, 1);
    evas_object_smart_callback_add(bt_speaker, "clicked", _switch_speaker, NULL);

    elm_object_disabled_set(bt_mute, TRUE);

    evas_object_show(m);
    return m;
}

void phone_call_win_call_remove(PhoneCallBlock* call)
{
    g_return_if_fail(call != NULL);

    g_debug("[PhoneCallWin] Removing call block for %d", call->id);

    // unica chiamata, chiudi finestra e deinizializza il suono
    if (eina_list_count(calls) == 1) {
        phone_call_win_hide();

        sound_state_set(SOUND_STATE_IDLE);
        sound_headset_presence_callback(NULL);

        ousaged_usage_release_resource("CPU", NULL, NULL);

        sound_volume_mute_set(CONTROL_MICROPHONE, 1);
        elm_button_label_set(bt_mute, _("Mute"));

        elm_object_disabled_set(bt_mute, TRUE);

        elm_button_label_set(bt_speaker, _("Speaker"));

        odeviced_idlenotifier_set_state(IDLE_STATE_IDLE_PRELOCK, NULL, NULL);
    }

    // remove call
    calls = eina_list_remove(calls, call);
    // destroy call
    phone_call_block_destroy(call);
    // update notification
    sync_call_notification();
}

/* richiesta nuova chiamata in uscita */
void phone_call_win_outgoing_call(const char* peer)
{
    g_debug("[PhoneCallWin] Outgoing call request to %s", peer);

    // metti in attesa le chiamate attive
    hold_all();

    // aggiungi la chiamata
    append_callblock(peer, 0, FALSE);

    // la notifica e' pushata dal callblock

    // attiva finestra
    phone_call_win_activate();
}

/* richiamato da gsm.c */
void phone_call_win_call_status(int id, CallStatus status, GHashTable* properties)
{
    g_return_if_fail(win != NULL && properties != NULL);
    g_debug("[PhoneCallWin/%d] Call status (only incoming): status=%d", id, status);

    const char *reason = map_get_string(properties, "reason");
    const char *number = map_get_string(properties, "number");
    Eina_List* iter;

    // mmm...
    if (number == NULL) {
        g_debug("[PhoneCallWin/%d] number property is NULL, trying peer property", id);
        number = map_get_string(properties, "peer");
    }

    g_debug("[PhoneCallWin/%d] peer = %s, reason = %s", id, number, reason);

    // chiamata entrante
    if (status == CALL_STATUS_INCOMING) {
        // controlla se la chiamata e' gia' stata inserita

        PhoneCallBlock* c = NULL;
        EINA_LIST_FOREACH(calls, iter, c)
            if (c->id == id) return; // ehm...

        // appendi il blocco chiamata
        c = append_callblock(number, id, FALSE);

        // segnale iniziale di arrivo chiamata entrante
        phone_call_block_call_status(c, CALL_STATUS_INCOMING, properties);

        // chiamata in arrivo -- attiva la finestra
        phone_call_win_activate();

        // notifica
        call_notification_start(eina_list_count(calls) > 1);

    } else {
        // sicuramente dobbiamo fermare qualunque notifica
        call_notification_stop();

        // altre chiamate, cerca in lista e inoltra il segnale

        PhoneCallBlock* c = NULL;
        EINA_LIST_FOREACH(calls, iter, c) {
            if (c->id == id) {
                phone_call_block_call_status(c, status, properties);
                c = NULL;
                break;
            }
        }

        // chiamata in uscita non trovata... mmm...
        if (c && status == CALL_STATUS_OUTGOING) {
            // appendi il blocco chiamata
            PhoneCallBlock *c = append_callblock(number, id, TRUE);

            // segnale iniziale di avvio chiamata in uscita
            phone_call_block_call_status(c, CALL_STATUS_OUTGOING, properties);

            // chiamata in uscita -- attiva la finestra
            phone_call_win_activate();
        }

        // e' la prima chiamata attiva, inizializza il suono
        if ((status == CALL_STATUS_ACTIVE || status == CALL_STATUS_OUTGOING) && eina_list_count(calls) == 1) {
            sound_state_set(SOUND_STATE_INIT);
            sound_headset_presence_callback(headset_presence);

            sound_volume_mute_set(CONTROL_MICROPHONE, 0);
            elm_button_label_set(bt_mute, _("Mute"));

            elm_object_disabled_set(bt_mute, FALSE);
        }

    }
}

int phone_call_win_num_calls(void)
{
    return eina_list_count(calls);
}

void phone_call_win_activate(void)
{
    g_return_if_fail(win != NULL);

    mokowin_activate(win);
}

void phone_call_win_hide(void)
{
    g_return_if_fail(win != NULL);

    mokowin_hide(win);
}

void phone_call_win_init(void)
{
    win = mokowin_new("mokocall", TRUE);
    if (win == NULL) {
        g_error("[PhoneCallWin] Cannot create main window. Exiting");
        return;
    }

    win->delete_callback = _delete;

    elm_win_title_set(win->win, _("Call"));
    elm_win_borderless_set(win->win, TRUE);

    mokowin_create_vbox(win, TRUE);
    mokowin_menu_enable(win);

    mokowin_menu_set(win, make_menu());

    /* altre cose */
    sound_set_mute_pointer(&current_mute_status);

    odeviced_vibrator_dbus_connect();

    if (odevicedVibratorBus == NULL)
        g_critical("Cannot connect to odeviced (vibrator). Will not be able to vibrate on incoming call");

    // valori iniziali settaggi
    gboolean value;

    if (remote_config_service_get_bool(phone_config, "phone", CALL_NOTIFICATION_SOUND, &value))
        call_notification_sound = value;

    if (remote_config_service_get_bool(phone_config, "phone", CALL_NOTIFICATION_VIBRATION, &value))
        call_notification_vibration = value;

    remote_config_service_get_string(phone_config, "phone", CALL_NOTIFICATION_RINGTONE, &call_notification_ringtone);

    // callback per i settaggi delle notifiche
    g_signal_connect(G_OBJECT(phone_config), "changed", G_CALLBACK(call_notification_settings), NULL);
}
