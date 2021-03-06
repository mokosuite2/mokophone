/* TODO
 * - segnali per le chiamate perse
 * - azioni per la notifica delle chiamate perse
 */

#include "globals.h"
#include "gsm.h"

#include <mokosuite/ui/gui.h>
#include <mokosuite/pim/contactsdb.h>
#include <mokosuite/pim/callsdb.h>
#include <mokosuite/utils/misc.h>
#include <mokosuite/utils/utils.h>
#include <mokosuite/utils/notify.h>
#include <freesmartphone-glib/opimd/call.h>

#include "phonewin.h"
#include "logview.h"
#include "logentry.h"
#include "callwin.h"

static Elm_Genlist_Item_Class itc = {0};
static Elm_Genlist_Item_Class itc_sub = {0};
Evas_Object *log_list = NULL;

// FIXME bug del cazzo della genlist
static gboolean longpressed = FALSE;

// lost calls list (for setting New=0)
static Eina_List* lost_calls = NULL;

// lost calls notification
static NotifyNotification* lost_calls_notification = NULL;
static bool dirty_notification = FALSE;

// callback data for update/delete signals for lost calls
typedef struct {
    CallEntry* call;
    gpointer update_signal;
    gpointer delete_signal;
} notification_signal_t;

static void sync_lost_calls_notification(void);


/* -- sezione log -- */

static void _list_selected(void *data, Evas_Object *obj, void *event_info)
{
    elm_genlist_item_selected_set((Elm_Genlist_Item*)event_info, FALSE);

    // FIXME bug del cazzo della genlist
    if (longpressed) {
        longpressed = FALSE;
        return;
    }

    // finestra gestione chiamata
    CallEntry *e = (CallEntry *)elm_genlist_item_data_get((const Elm_Genlist_Item *)event_info);

    if (e != NULL)
        logentry_new(e);
}

static void _popup_click(gpointer popup, gpointer data, int index, gboolean final)
{
    CallEntry *c = (CallEntry *) data;

    g_debug("POPUP! Click to %d, final = %d", index, final);
    switch (index) {

        // chiamata
        case 1:
            if (c->peer != NULL)
                phone_win_call_internal(c->peer, NULL);

            break;

        // cancella entry
        case 2:
            callsdb_delete_call(c->id);
            elm_genlist_item_del((Elm_Genlist_Item *)c->data);

            break;

        default:
            break;
    }
}

// wrapper per logentry
static void _popup_clicked_wrapper(CallEntry* c, int choose)
{
    _popup_click(NULL, c, choose, TRUE);
}

static void _list_longpressed(void *data, Evas_Object *obj, void *event_info)
{
    // FIXME bug del cazzo della genlist
    longpressed = TRUE;

    CallEntry *e = (CallEntry *)elm_genlist_item_data_get((const Elm_Genlist_Item *)event_info);

    MokoPopupMenu *p = moko_popup_menu_new(phone_win_get_mokowin(), NULL, MOKO_POPUP_BUTTONS, _popup_click, e);

    if (e->peer != NULL) {
        char *s = g_strdup_printf(_("Call %s"), e->peer);
        moko_popup_menu_add(p, s, 1, FALSE);
        g_free(s);
    }

    moko_popup_menu_add(p, _("Delete log entry"), 2, FALSE);

    if (e->peer != NULL)
        moko_popup_menu_add(p, _("Add to contacts"), 3, FALSE);

    mokoinwin_activate(MOKO_INWIN(p));
}

static void _log_clear_clicked(void *data, Evas_Object *obj, void *event_info)
{
    // nascondi il menu
    mokowin_menu_hide(phone_win_get_mokowin());

    // TODO chiedi conferma?
    callsdb_truncate();
    elm_genlist_clear(log_list);
}

static void _log_call_clicked(void *data, Evas_Object *obj, void *event_info)
{
    CallEntry *c = (CallEntry *) data;

    if (c->peer != NULL)
        phone_win_call_internal(c->peer, NULL);
}

static char* log_genlist_label_get(void *data, Evas_Object * obj, const char *part)
{
    // TODO
    CallEntry* call = (CallEntry *) data;
    //g_debug("Requesting label for part %s (data = %p)", part, data);

    if (!strcmp(part, "elm.text")) {

        if (call->data2) {
            ContactField* f = contactsdb_get_first_field((ContactEntry *) call->data2, CONTACT_FIELD_NAME);
            return g_strdup( (f != NULL) ? f->value : _("(no name)"));
        }

        else {

            if (call->peer)
                return g_strdup(call->peer);
            else
                return g_strdup(_("(no number)"));
        }

    } else if (!strcmp(part, "elm.text.sub")) {

        if (call->peer)
            return g_strdup(call->peer);
        else
            return g_strdup(_("(no number)"));

    } else if (!strcmp(part, "elm.text.right")) {

        return get_time_repr(call->timestamp);
    }


    return NULL;
}

static Evas_Object* log_genlist_icon_get(void *data, Evas_Object * obj, const char *part)
{
    //g_debug("Requesting icon for part %s (data = %p)", part, data);

    CallEntry* call = (CallEntry *) data;
    MokoWin* win = phone_win_get_mokowin();

    if (!strcmp(part, "elm.swallow.icon")) {

        // icona bt_call
        Evas_Object *icon_call = elm_icon_add(win->win);
        elm_icon_file_set(icon_call, MOKOPHONE_DATADIR "/call-start.png", NULL);

        elm_icon_smooth_set(icon_call, TRUE);
        elm_icon_scale_set(icon_call, TRUE, TRUE);
        evas_object_show(icon_call);

        // bt_call
        Evas_Object *bt_call = elm_button_add(win->win);
        elm_button_icon_set(bt_call, icon_call);

        evas_object_propagate_events_set(bt_call, FALSE);

        if (call->peer != NULL)
            evas_object_smart_callback_add(bt_call, "clicked", _log_call_clicked, data);
        else
            elm_object_disabled_set(bt_call, TRUE);

        return bt_call;
    }

    else if (!strcmp(part, "elm.swallow.end")) {

        Evas_Object *icon_type = elm_icon_add(win->win);

        char* type = NULL;

        if (call->direction == DIRECTION_OUTGOING)
            type = "out";

        else if (call->direction == DIRECTION_INCOMING) {

            if (call->answered) type = "in";
            else type = "missed";
        }

        char* file = g_strdup_printf(MOKOPHONE_DATADIR "/log_call-%s.png", type);

        elm_icon_file_set(icon_type, file, NULL);

        g_free(file);

        //elm_icon_smooth_set(icon_type, TRUE);
        elm_icon_no_scale_set(icon_type, TRUE);
        elm_icon_scale_set(icon_type, FALSE, TRUE);
        evas_object_show(icon_type);

        return icon_type;
    }

    return NULL;
}

static Eina_Bool log_genlist_state_get(void *data, Evas_Object * obj, const char *part)
{
    // TODO
    return FALSE;
}

static void log_genlist_del(void *data, Evas_Object *obj)
{
    CallEntry* call = (CallEntry *) data;
    g_free(call->peer);
    g_free(call);
}

static void call_update(gpointer data, GHashTable* props)
{
    g_debug("Call has been modified - checking New attribute");
    // chiamata modificata - controlla new
    if (g_hash_table_lookup(props, "New") && !map_get_bool(props, "New", TRUE)) {
        notification_signal_t* no = data;

        // disconnetti segnali fso
        opimd_call_call_updated_disconnect(no->update_signal);
        opimd_call_call_deleted_disconnect(no->delete_signal);

        g_debug("New is 0 - removing missed call");
        lost_calls = eina_list_remove(lost_calls, no->call);
        dirty_notification = TRUE;
        sync_lost_calls_notification();

        g_free(no);
    }
}

static void call_remove(gpointer data)
{
    notification_signal_t* no = data;

    // chiamata cancellata - rimuovi sicuramente
    g_debug("Call has been deleted - removing missed call");

    // disconnetti segnali fso
    opimd_call_call_updated_disconnect(no->update_signal);
    opimd_call_call_deleted_disconnect(no->delete_signal);

    lost_calls = eina_list_remove(lost_calls, no->call);
    dirty_notification = TRUE;
    sync_lost_calls_notification();

    g_free(no);
}

Elm_Genlist_Item_Class* log_preprocess_call(CallEntry* call)
{
    Elm_Genlist_Item_Class* cur_itc = &itc;
    if (call->peer) {
        call->data2 = contactsdb_lookup_number(call->peer);

        if (call->data2)
            cur_itc = &itc_sub;
    }

    // appendi l'id della chiamata alla lista se persa
    if (!call->answered && call->is_new && call->direction == DIRECTION_INCOMING) {
        g_debug("Adding lost call %d", call->id);
        Eina_List* iter;
        CallEntry* cp;

        EINA_LIST_FOREACH(lost_calls, iter, cp)
            if (cp->id == call->id) goto skip_lost;

        dirty_notification = TRUE;
        lost_calls = eina_list_append(lost_calls, call);

        // fso signals for updates and delete
        notification_signal_t* no = calloc(1, sizeof(notification_signal_t));
        no->call = call;
        char* path = callsdb_get_call_path(call->id);
        no->delete_signal = opimd_call_call_deleted_connect(path, call_remove, no);
        no->update_signal = opimd_call_call_updated_connect(path, call_update, no);
skip_lost:
        ;
    }

    return cur_itc;
}

static void _notify_activate(NotifyNotification* notify, char* action, void* data)
{
    // show call win
    phone_win_activate(SECTION_LOG, FALSE);
}

static void sync_lost_calls_notification(void)
{
    if (!dirty_notification) return;
    dirty_notification = FALSE;

    char* msg;
    char* body;
    int count = eina_list_count(lost_calls);
    Eina_List* iter;
    CallEntry* call;

    // no calls - remove if present
    if (!count) {
        if (lost_calls_notification) {
            notify_notification_close(lost_calls_notification, NULL);
            g_object_unref(lost_calls_notification);
            lost_calls_notification = NULL;
        }
        return;
    }

    // one call
    else if (count == 1) {
        msg = g_strdup(_("1 missed call"));
        // TODO body: date/time or peer?
        CallEntry* call = lost_calls->data;
        body = g_strdup(call->peer);
    }
    else {
        msg = g_strdup_printf(_("%d missed calls"), count);
        char** content = calloc(count + 1, sizeof(char *));
        int i = 0;
        EINA_LIST_FOREACH(lost_calls, iter, call)
            content[i++] = call->peer;

        body = g_strjoinv("\n", content);
        g_free(content);
    }

    if (!lost_calls_notification) {
        lost_calls_notification = mokosuite_notification_new(
            "phone.call.lost",      // category
            msg,                    // summary
            body,                   // body
            NULL,                   // icon TODO
            NOTIFICATION_HINT_SHOW_ON_RESUME     // show on resume
        );
        notify_notification_set_timeout(lost_calls_notification, 0);
        notify_notification_set_hint_string(lost_calls_notification, "image_path", "file://" MOKOPHONE_DATADIR "/log_call-missed.png");
        notify_notification_add_action(lost_calls_notification, "activate", _("Show"), NOTIFY_ACTION_CALLBACK(_notify_activate), NULL, NULL);
    }
    else
        notify_notification_update(lost_calls_notification, msg, body, NULL);

    g_free(msg);
    g_free(body);

    notify_notification_show(lost_calls_notification, NULL);
}

// richiamato all'aggiunta di una chiamata appena terminata
static void log_process_call_first(CallEntry* call, gpointer data)
{
    Elm_Genlist_Item_Class* cur_itc = log_preprocess_call(call);
    call->data = elm_genlist_item_prepend((Evas_Object *) data, cur_itc, call,
        NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

    sync_lost_calls_notification();
}

// richiamato all'aggiunta di una chiamata in caricamento dal db
static void log_process_call(CallEntry* call, gpointer data)
{
    Elm_Genlist_Item_Class* cur_itc = log_preprocess_call(call);
    call->data = elm_genlist_item_append((Evas_Object *) data, cur_itc, call,
        NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

    sync_lost_calls_notification();
}

/* costruisce la sezione log */
Evas_Object* logview_make_section(void)
{
    // catturiamo la finestra :D
    MokoWin* win = phone_win_get_mokowin();

    // overlay per gli elementi della lista del log
    elm_theme_overlay_add(NULL, "elm/genlist/item/call/default");
    elm_theme_overlay_add(NULL, "elm/genlist/item_odd/call/default");

    log_list = elm_genlist_add(win->win);
    evas_object_size_hint_weight_set(log_list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    elm_genlist_bounce_set(log_list, FALSE, FALSE);
    evas_object_smart_callback_add(log_list, "selected", _list_selected, NULL);
    evas_object_smart_callback_add(log_list, "longpressed", _list_longpressed, NULL);

    itc.item_style = "call";
    itc.func.label_get = log_genlist_label_get;
    itc.func.icon_get = log_genlist_icon_get;
    itc.func.state_get = log_genlist_state_get;
    itc.func.del = log_genlist_del;

    itc_sub.item_style = "call_sub";
    itc_sub.func.label_get = log_genlist_label_get;
    itc_sub.func.icon_get = log_genlist_icon_get;
    itc_sub.func.state_get = log_genlist_state_get;
    itc_sub.func.del = log_genlist_del;

    evas_object_show(log_list);

    // inizializza il database delle chiamate
    callsdb_init(log_process_call_first, log_list);

    // carica subito le chiamate
    callsdb_foreach_call(log_process_call, log_list);

    // inializza quell'altro
    logentry_init(_popup_clicked_wrapper);

    sync_lost_calls_notification();
    return log_list;
}

/* costruisce il menu della sezione log */
Evas_Object* logview_make_menu(void)
{
    MokoWin* win = phone_win_get_mokowin();

    Evas_Object *bt_clear = elm_button_add(win->win);
    elm_button_label_set(bt_clear, _("Clear log"));

    evas_object_size_hint_weight_set(bt_clear, 1.0, 0.0);
    evas_object_size_hint_align_set(bt_clear, -1.0, 1.0);

    evas_object_smart_callback_add(bt_clear, "clicked", _log_clear_clicked, NULL);

    return bt_clear;
}

void logview_reset_view(void)
{
    Elm_Genlist_Item *item = elm_genlist_first_item_get(log_list);

    if (item)
        elm_genlist_item_show(item);

    // imposta tutte le chiamate perse nuove a New = 0
    Eina_List* iter;
    CallEntry* call;

    EINA_LIST_FOREACH(lost_calls, iter, call)
        callsdb_set_call_new(call->id, FALSE);

    eina_list_free(lost_calls);
    lost_calls = NULL;
}
