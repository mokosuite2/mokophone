
#include "globals.h"
#include "callwin.h"
#include "callblock.h"
#include "logview.h"

#include <mokosuite/pim/contactsdb.h>
#include <mokosuite/utils/misc.h>
#include <mokosuite/utils/utils.h>
#include <freesmartphone-glib/freesmartphone-glib.h>
#include <freesmartphone-glib/ogsmd/call.h>

#include <Elementary.h>

#define split_duration(x) g_strdup_printf("%02u:%02u:%02u", (guint)(x / 3600), (guint)(x / 60), (guint)(x % 60))

static void configure_call(PhoneCallBlock* call, bool outgoing);
static void create_bubble(PhoneCallBlock* call, bool outgoing);
static void init_incoming(PhoneCallBlock* call);
static void init_outgoing(PhoneCallBlock* call, bool initiate);

static gboolean insert_call(gpointer data);
static void insert_call_post(PhoneCallBlock* call);

static void end_call(PhoneCallBlock* call, bool release);
static void call_held(PhoneCallBlock* call);
static void call_duration_update(PhoneCallBlock* c);


/* chiamata finalizzata */
static void _finalize_call(gpointer popup, gpointer data)
{
    phone_call_win_call_remove(PHONE_CALL_BLOCK(data));
}

static void _reject_callback(GError *error, void *data)
{
    if (error != NULL) g_warning("Reject error: %s", error->message);

    g_idle_add(insert_call, data);
}

static void _accept_callback(GError *error, void *data)
{
    if (error != NULL) {
        g_warning("Activate error: %s", error->message);
        moko_popup_alert_new_with_callback(PHONE_CALL_BLOCK(data)->parent, _("Cannot activate voice call."), _finalize_call, data);
    }

    // il resto e' gestito da CallStatus
}

/* abbiamo messo una chiamata in attesa */
static void _hold_callback(GError *error, void *data)
{
    PhoneCallBlock *call = PHONE_CALL_BLOCK(data);

    if (error != NULL) {
        g_warning("Hold error: %s", error->message);
        moko_popup_alert_new(call->parent, _("Cannot hold voice call."));

    } else {
        // forza lo stato per coerenza
        call->status = CALL_STATUS_HELD;
    }

    call_held(call);
}

/* abbiamo riattivato una chiamata in attesa */
static void _held_to_active_callback(GError *error, void *data)
{
    PhoneCallBlock *call = PHONE_CALL_BLOCK(data);

    if (error != NULL) {
        g_warning("Activate from held error: %s", error->message);
        moko_popup_alert_new(call->parent, _("Cannot activate voice call."));

    } else {
        // forza lo stato per coerenza
        call->status = CALL_STATUS_ACTIVE;
    }

    call_held(call);
}

static void _voice_call_callback(GError *error, int call_id, void *data)
{
    if (error != NULL) {
        g_warning("Initiate error: %s", error->message);

        // chiamata in chiusura...
        end_call(PHONE_CALL_BLOCK(data), FALSE);

        char *errmsg = NULL;

        // segnale GSM assente/insufficiente
        #if 0
        if (IS_CALL_ERROR(error, CALL_ERROR_NO_CARRIER))
            errmsg = "No GSM carrier.";
        else
        #endif
            errmsg = "Cannot initiate voice call.";

        // mostra errore...
        moko_popup_alert_new_with_callback(PHONE_CALL_BLOCK(data)->parent, _(errmsg), _finalize_call, data);
        return;
    }

    g_debug("Voice call %d initiated", call_id);
    PHONE_CALL_BLOCK(data)->id = call_id;

    // posticipato a stato active -- edje_object_signal_emit(PHONE_CALL_BLOCK(data)->layout_edje, "active", "call");
}

/* chiamata chiusa */
static void _release_callback(GError *error, void *data)
{
    if (error != NULL) g_warning("Release error: %s", error->message);

    PhoneCallBlock *call = PHONE_CALL_BLOCK(data);

    // disattiva i controlli
    elm_object_disabled_set(call->button_hold, TRUE);
    elm_object_disabled_set(call->button_release, TRUE);

    // chiamata terminata
    edje_object_signal_emit(call->layout_edje, "release", "call");

    call_duration_update(call);

    g_idle_add(insert_call, call);
}

/* callback pulsante reject */
static void _reject_clicked(void *data, Evas_Object *obj, void *event_info)
{
    int mute = (int) evas_object_data_get(obj, "mute_button");

    // mute button, stop notifications
    if (mute) {
        phone_call_win_notification_stop();
        elm_button_label_set(obj, _("Reject"));
        evas_object_data_set(obj, "mute_button", (void*) FALSE);
    }

    // reject call
    else
        ogsmd_call_release(PHONE_CALL_BLOCK(data)->id, _reject_callback, data);
}

/* callback pulsante hold */
static void _hold_clicked(void *data, Evas_Object *obj, void *event_info)
{
    PhoneCallBlock *call = PHONE_CALL_BLOCK(data);

    if (call->status == CALL_STATUS_HELD)
    {
        // disabilita pulsante
        elm_object_disabled_set(call->button_hold, TRUE);

        // chiamata in attesa -- attivala
        //ogsmd_call_activate(call->id, _held_to_active_callback, call);
        ogsmd_call_hold_active(_held_to_active_callback, call);
    }
    else if(call->status == CALL_STATUS_ACTIVE) {
        // disabilita pulsante
        elm_object_disabled_set(call->button_hold, TRUE);

        // chiamata attiva -- metti in attesa
        ogsmd_call_hold_active(_hold_callback, call);
    }

}

/* callback pulsante release */
static void _release_clicked(void *data, Evas_Object *obj, void *event_info)
{
    end_call(PHONE_CALL_BLOCK(data), TRUE);
}

static void _accept_slider(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    PhoneCallBlock *call = PHONE_CALL_BLOCK(data);

    if (!call->accepted) {
        call->accepted = TRUE;

        g_debug("Accepting call %d", call->id);

        // disattiva pulsante reject
        elm_object_disabled_set(call->button_reject, TRUE);

        ogsmd_call_activate(call->id, _accept_callback, call);
    }
}

static void call_held(PhoneCallBlock* call)
{
    g_return_if_fail(call != NULL);

    // ferma/ripristina timer
    // FIXME la chiamata in effetti continua anche se trattenuta
    //((call->status == CALL_STATUS_ACTIVE) ? g_timer_continue : g_timer_stop)(call->timer);

    // TODO cambia icona pulsante hold (?)

    edje_object_signal_emit(call->layout_edje,
        (call->status == CALL_STATUS_ACTIVE) ? "active" : "hold", "call");

    elm_object_disabled_set(call->button_hold, FALSE);
}

static void end_call(PhoneCallBlock* call, bool release)
{
    g_return_if_fail(call != NULL);

    if (release)
        ogsmd_call_release(call->id, NULL, NULL);
}

static void call_duration_update(PhoneCallBlock* c)
{
    if (c->timer != NULL)
    {
        c->duration = (unsigned long)g_timer_elapsed(c->timer, NULL);

        // gia' che ci siamo distruggiamo il timer
        g_timer_destroy(c->timer);
        c->timer = NULL;
    }
}

void insert_call_post(PhoneCallBlock* call)
{
    g_return_if_fail(call != NULL);

    // chiamata in chiusura e annullata da CallStatus, mostra popup
    if (call->id == 0 && call->status == CALL_STATUS_RELEASE && call->answered) {

        char* fmt = split_duration(call->duration);
        char *msg = g_strdup_printf(_("Duration: %s"), fmt);

        moko_popup_alert_new_with_callback(call->parent, msg, _finalize_call, call);

        g_free(fmt);
        g_free(msg);

    } else {
        _finalize_call(NULL, call);
    }
}

static gboolean update_timer(gpointer data)
{
    PhoneCallBlock *c = PHONE_CALL_BLOCK(data);

    // il nostro compito e' finito
    if (c->timer == NULL || c->duration > 0) return FALSE;

    char* fmt = split_duration((guint)g_timer_elapsed(c->timer, NULL));
    edje_object_part_text_set(c->layout_edje, "status", fmt);
    g_free(fmt);

    return TRUE;
}

static gboolean insert_call(gpointer data)
{
    PhoneCallBlock *c = PHONE_CALL_BLOCK(data);

    // ehm... la durata...
    call_duration_update(c);

    // rimuovi l'eventuale notifica
    // TODO libnotify

    // tutto gestito da opimd!
    insert_call_post(c);
    return FALSE;
}


// passare outgoing a TRUE anche se chiamata entrante, purche' sia accettata
static void configure_call(PhoneCallBlock* call, bool outgoing)
{
    if (outgoing) {
        g_debug("Configuring outgoing/active call");

        // distruggi i vecchi controlli
        if (call->button_reject != NULL) {
            evas_object_del(call->button_reject);
            call->button_reject = NULL;
        }

        elm_layout_file_set(call->layout, MOKOPHONE_DATADIR "/theme.edj", "mokosuite/phone/call/active");

        /* icona pulsante hold */
        Evas_Object *icon_hold = elm_icon_add(call->parent->win);
        elm_icon_file_set(icon_hold, MOKOPHONE_DATADIR "/call-hold.png", NULL);

        elm_icon_smooth_set(icon_hold, TRUE);
        elm_icon_scale_set(icon_hold, TRUE, TRUE);
        evas_object_show(icon_hold);

        /* pulsante hold */
        call->button_hold = elm_button_add(call->parent->win);
        elm_button_icon_set(call->button_hold, icon_hold);

        evas_object_size_hint_weight_set(call->button_hold, 1.0, 1.0);

        evas_object_smart_callback_add(call->button_hold, "clicked", _hold_clicked, call);

        evas_object_show(call->button_hold);
        elm_layout_content_set(call->layout, "hold", call->button_hold);

        /* icona pulsante release */
        Evas_Object *icon_release = elm_icon_add(call->parent->win);
        elm_icon_file_set(icon_release, MOKOPHONE_DATADIR "/call-end.png", NULL);

        elm_icon_smooth_set(icon_release, TRUE);
        elm_icon_scale_set(icon_release, TRUE, TRUE);
        evas_object_show(icon_release);

        /* pulsante release */
        call->button_release = elm_button_add(call->parent->win);
        elm_button_icon_set(call->button_release, icon_release);

        evas_object_size_hint_weight_set(call->button_release, 1.0, 1.0);

        evas_object_smart_callback_add(call->button_release, "clicked", _release_clicked, call);

        evas_object_show(call->button_release);
        elm_layout_content_set(call->layout, "release", call->button_release);

    } else {
        g_debug("Configuring incoming call");

        elm_layout_file_set(call->layout, MOKOPHONE_DATADIR "/theme.edj", "mokosuite/phone/call/incoming");

        /* pulsante reject */
        // FIXME se c'e' una chiamata terminata non chiusa, qua non funziona (e nemmeno la notifica)
        int mute_button = (call_notification_sound && phone_call_win_num_calls() < 1);
        call->button_reject = elm_button_add(call->parent->win);
        elm_button_label_set(call->button_reject, _(mute_button ? "Mute" : "Reject"));
        evas_object_data_set(call->button_reject, "mute_button", (void*) mute_button);

        evas_object_size_hint_weight_set(call->button_reject, 1.0, 1.0);

        evas_object_smart_callback_add(call->button_reject, "clicked", _reject_clicked, call);

        evas_object_show(call->button_reject);
        elm_layout_content_set(call->layout, "reject", call->button_reject);

        /* segnali per lo scroll freeze */
        call->data_freeze = g_new0(ScrollCallbackData, 1);
        call->data_freeze->win = call->parent;
        call->data_freeze->freeze = TRUE;

        edje_object_signal_callback_add(call->layout_edje, "beginDrag", "slider:slider",
            (Edje_Signal_Cb) mokowin_scroll_freeze_set_callback, call->data_freeze);

        call->data_unfreeze = g_new0(ScrollCallbackData, 1);
        call->data_unfreeze->win = call->parent;
        call->data_unfreeze->freeze = FALSE;

        edje_object_signal_callback_add(call->layout_edje, "finishDrag", "slider:slider",
            (Edje_Signal_Cb) mokowin_scroll_freeze_set_callback, call->data_unfreeze);

        /* segnale di accettazione dallo slider */
        edje_object_signal_callback_add(call->layout_edje, "unlockScreen", "slider:slider", _accept_slider, call);
    }

    if (call->peer) {
        edje_object_part_text_set(call->layout_edje, "number", call->peer);

        ContactEntry* e = contactsdb_lookup_number(call->peer);
        char* name_str = NULL;
        if (e) {
            ContactField* f = contactsdb_get_first_field(e, CONTACT_FIELD_NAME);
            name_str = (f != NULL) ? f->value : NULL;
        }

        edje_object_part_text_set(call->layout_edje, "name", (name_str != NULL) ? name_str : _("(no name)"));
    }

    else {
        edje_object_part_text_set(call->layout_edje, "name", _("(no name)"));
        edje_object_part_text_set(call->layout_edje, "number", _("(no number)"));
    }

}

static void create_bubble(PhoneCallBlock* call, bool outgoing)
{
    call->widget = elm_bubble_add(call->parent->win);
    evas_object_size_hint_weight_set(call->widget, 1.0, 1.0);
    evas_object_size_hint_align_set(call->widget, -1.0, 0.0);
    evas_object_show(call->widget);

    elm_bubble_corner_set(call->widget, (outgoing) ? "bottom_left" : "bottom_right");

    call->layout = elm_layout_add(call->parent->win);
    //evas_object_size_hint_weight_set(call->layout, 1.0, 1.0);
    evas_object_show(call->layout);

    call->layout_edje = elm_layout_edje_get(call->layout);

    /* configura la chiamata */
    configure_call(call, outgoing);

    edje_object_signal_emit(call->layout_edje, "dialing", "call");
    edje_object_part_text_set(call->layout_edje, "status", _("Dialing..."));

    elm_bubble_content_set(call->widget, call->layout);
}

static void init_incoming(PhoneCallBlock* call)
{
    create_bubble(call, FALSE);
}

static void init_outgoing(PhoneCallBlock* call, bool initiate)
{
    create_bubble(call, TRUE);

    // inizia chiamata
    if (initiate)
        ogsmd_call_initiate(call->peer, "voice", _voice_call_callback, call);
    else
        _voice_call_callback(NULL, call->id, call);
}

void phone_call_block_update_hold(PhoneCallBlock* call)
{
    call_held(call);
}

void phone_call_block_call_status(PhoneCallBlock* call, const CallStatus status, GHashTable* properties)
{
    g_return_if_fail(call != NULL && properties != NULL);
    g_debug("[PhoneCallBlock/%d/%p] Received CallStatus %d (status=%d, old_status=%d)", call->id, call, status, call->status, call->old_status);

    int previous_status = call->status;

    // stato iniziale -- salvalo in old_status
    if (call->status < 0)
        call->old_status = status;

    call->status = status;

    switch(status)
    {
        case CALL_STATUS_INCOMING:
            call->timestamp = get_current_time();

            break;

        case CALL_STATUS_OUTGOING:
            call->timestamp = get_current_time();
            edje_object_part_text_set(call->layout_edje, "status", _("Connecting..."));

            break;

        case CALL_STATUS_ACTIVE:
            call->answered = TRUE;

            // la chiamata era in attesa
            if (previous_status == CALL_STATUS_HELD) {
                call_held(call);
            }
            else {
                // il timer parte subito
                call->timer = g_timer_new();
            }

            // timeout idler per rinnovo tempo (eeeh?!?)
            bool create_time_idler = FALSE;

            // la chiamata era in entrata
            if (previous_status == CALL_STATUS_INCOMING) {
                configure_call(call, TRUE);
                create_time_idler = TRUE;
            } else {
                edje_object_signal_emit(call->layout_edje, "active", "call");
                create_time_idler = TRUE;
            }

            if (create_time_idler) {
                update_timer(call); // triggerane uno subito va...
                g_timeout_add(500, update_timer, call);
            }

            break;

        case CALL_STATUS_HELD:
            call_held(call);

            break;

        case CALL_STATUS_RELEASE:
            // inibisci i segnali per la chiamata
            call->id = 0;

            // chiusura chiamata
            end_call(call, FALSE);

            // distruzione chiamata
            _release_callback(NULL, call);


            break;

        default:
            // TODO
            break;
    }

}

void phone_call_block_destroy(PhoneCallBlock* call)
{
    g_return_if_fail(call != NULL);

    g_free(call->data_freeze);
    g_free(call->data_unfreeze);

    if (call->button_reject != NULL)
        evas_object_del(call->button_reject);

    if (call->widget != NULL)
        evas_object_del(call->widget);

    if (call->timer != NULL)
        g_timer_destroy(call->timer);

    g_free(call->peer);
    g_free(call);
}

PhoneCallBlock* phone_call_block_new(MokoWin* parent, const char* peer, int id, bool outgoing)
{
    PhoneCallBlock* b = g_new0(PhoneCallBlock, 1);

    /* cose varie */
    b->parent = parent;
    b->id = id;
    b->peer = g_strdup(peer);

    b->status = b->old_status = -1;

    if (id > 0 && !outgoing) {
        // chiamata entrante
        init_incoming(b);

    } else {
        // chiamata uscente
        init_outgoing(b, !outgoing);
    }

    // pusha la notifica
    // TODO libnotify

    return b;
}
