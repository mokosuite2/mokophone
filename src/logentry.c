#include "globals.h"

#include <mokosuite/ui/gui.h>
#include <mokosuite/utils/misc.h>
#include <mokosuite/utils/utils.h>
#include <mokosuite/pim/contactsdb.h>
#include <mokosuite/pim/callsdb.h>
#include <Elementary.h>
#include "logentry.h"


#define split_duration(x) g_strdup_printf("%02u:%02u:%02u", (guint)(x / 3600), (guint)(x / 60), (guint)(x % 60))

static void (*return_cb)(CallEntry*, int);

static void _close(void* data, Evas_Object* obj, void* event_info)
{
    // distruggi tutto!!!
    mokowin_destroy(MOKO_WIN(data));
}

static void call_clicked(void* data, Evas_Object* obj, void* event_info)
{
    MokoWin* w = MOKO_WIN(data);
    CallEntry* c = (CallEntry*) w->data;

    return_cb(c, 1);
    _close(w, NULL, NULL);
}

static void delete_clicked(void* data, Evas_Object* obj, void* event_info)
{
    MokoWin* w = MOKO_WIN(data);
    CallEntry* c = (CallEntry*) w->data;

    return_cb(c, 2);
    _close(w, NULL, NULL);
}

void logentry_new(CallEntry* c)
{
    MokoWin* w = mokowin_new("logentry", TRUE);
    if (!w) {
        g_critical("Cannot create log entry window.");
        return; // sigh :(
    }

    // eheh :D
    w->data = c;
    w->delete_callback = _close;

    elm_win_title_set(w->win, _("Call log"));
    //elm_win_borderless_set(w->win, TRUE);

    mokowin_create_vbox(w, TRUE);

    // padding frame per info
    Evas_Object *frame = elm_frame_add(w->win);
    elm_object_style_set(frame, "pad_large");

    evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(frame);

    // dati chiamata (nome, numero, data e ora, durata)
    Evas_Object *info = elm_anchorblock_add(w->win);
    evas_object_size_hint_weight_set(info, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(info, EVAS_HINT_FILL, EVAS_HINT_FILL);

    // TODO elm_entry_context_menu_disabled_set(info, TRUE);
    char *time = get_time_repr_full(c->timestamp);

    char* name = NULL;
    if (c->data2) {
        ContactField* f = contactsdb_get_first_field((ContactEntry *) c->data2, CONTACT_FIELD_NAME);
        if (f) name = f->value;
    }

    char* fmt = split_duration(c->duration);
    char *duration = g_strdup_printf(_("Duration: %s"), fmt);

    char *s = g_strdup_printf( _("%s<br>%s<br>%s<br>%s" ),
        (name != NULL) ? name : _("(no name)"),
        (c->peer) ? c->peer : _("(no number)"),
        time,
        duration);

    elm_anchorblock_text_set(info, s);

    g_free(duration);
    g_free(fmt);
    g_free(time);
    g_free(s);

    evas_object_show(info);

    elm_frame_content_set(frame, info);
    elm_box_pack_start(w->vbox, frame);

    // pulsanti azioni
    if (c->peer) {
        /*char* */ s = g_strdup_printf(_("Call %s"), c->peer);

        mokowin_vbox_button_with_callback(w, s, NULL, NULL, call_clicked, w);
        g_free(s);
    }

    mokowin_vbox_button_with_callback(w, _("Delete log entry"), NULL, NULL, delete_clicked, w);
    // TODO mokowin_vbox_button_with_callback(w, _("Add to contacts"), NULL, NULL, delete_clicked, w);

    mokowin_activate(w);
}

void logentry_init(void (*choose_callback)(CallEntry*, int))
{
    return_cb = choose_callback;
}
