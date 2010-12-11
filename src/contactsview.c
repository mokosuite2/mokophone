#include "globals.h"
#include "gsm.h"

#include <mokosuite/ui/gui.h>
#include <mokosuite/utils/misc.h>
#include <mokosuite/utils/utils.h>
#include <mokosuite/pim/contactsdb.h>

#include "contactsview.h"
#include "phonewin.h"
#include "callwin.h"


static Elm_Genlist_Item_Class itc = {0};
Evas_Object* contacts_list = NULL;

// FIXME bug del cazzo della genlist
static gboolean longpressed = FALSE;

static void _list_selected(void *data, Evas_Object *obj, void *event_info)
{
    elm_genlist_item_selected_set((Elm_Genlist_Item*)event_info, FALSE);

    // FIXME bug del cazzo della genlist
    if (longpressed) {
        longpressed = FALSE;
        return;
    }

    // TODO finestra gestione chiamata
    #if 0
    ContactEntry *e = (CallEntry *)elm_genlist_item_data_get((const Elm_Genlist_Item *)event_info);

    if (e != NULL)
        logentry_new(e);
    #endif
}

static void _contacts_import_clicked(void* data, Evas_Object* obj, void* event_info)
{
    // nascondi il menu
    mokowin_menu_hide(phone_win_get_mokowin());

    // TODO apri finestra contatti sim
}

static void _contacts_call_clicked(void *data, Evas_Object *obj, void *event_info)
{
    ContactField* f = (ContactField *) data;

    if (f != NULL)
        phone_win_call_internal(f->value, NULL);
}

static char* contacts_genlist_label_get(void *data, Evas_Object * obj, const char *part)
{
    ContactEntry* c = (ContactEntry *)data;

    if (!strcmp(part, "elm.text")) {

        ContactField* f = contactsdb_get_first_field(c, CONTACT_FIELD_NAME);
        return g_strdup((f != NULL) ? f->value : _("(no name)") );

    } else if (!strcmp(part, "elm.text.sub")) {

        ContactField* f = contactsdb_get_default_field(c, CONTACT_DEFAULT_PHONE);
        return g_strdup((f != NULL) ? f->value : _("(no number)"));
    }


    return NULL;
}

static Evas_Object* contacts_genlist_icon_get(void *data, Evas_Object * obj, const char *part)
{
    ContactEntry* c = (ContactEntry *)data;
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

        ContactField* f = contactsdb_get_default_field(c, CONTACT_DEFAULT_PHONE);

        if (f != NULL)
            evas_object_smart_callback_add(bt_call, "clicked", _contacts_call_clicked, f);
        else
            elm_object_disabled_set(bt_call, TRUE);

        return bt_call;
    }

    return NULL;
}

static void contacts_genlist_del(void *data, Evas_Object *obj)
{
    // TODO bisogna chiamare la liberazione della cache di lookup!!!
    // TODO meglio far liberare tutto da contactsdb.c
    ContactEntry* e = (ContactEntry *)data;
    g_ptr_array_unref(e->fields);
    g_free(e);
}

static void contactsview_process_contact(ContactEntry* e, gpointer data)
{
    Elm_Genlist_Item *it = elm_genlist_item_append((Evas_Object *) data, &itc, e,
        NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);

    e->data = it;
}

/* costruisce la sezione contatti */
Evas_Object* contactsview_make_section(void)
{
    // catturiamo la finestra :D
    MokoWin* win = phone_win_get_mokowin();

    // overlay per gli elementi della lista dei contatti
    elm_theme_overlay_add(NULL, "elm/genlist/item/contact/default");
    elm_theme_overlay_add(NULL, "elm/genlist/item_odd/contact/default");

    contacts_list = elm_genlist_add(win->win);
    evas_object_size_hint_weight_set(contacts_list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    elm_genlist_bounce_set(contacts_list, FALSE, FALSE);
    evas_object_smart_callback_add(contacts_list, "selected", _list_selected, NULL);
    //evas_object_smart_callback_add(contacts_list, "longpressed", _list_longpressed, NULL);

    itc.item_style = "contact";
    itc.func.label_get = contacts_genlist_label_get;
    itc.func.icon_get = contacts_genlist_icon_get;
    itc.func.del = contacts_genlist_del;

    evas_object_show(contacts_list);

    // carica i contatti
    contactsdb_foreach_contact(
#ifdef CONTACTSDB_SQLITE
    CONTACT_FIELD_NAME, CONTACT_DEFAULT_PHONE,
#else
    NULL,
#endif
    contactsview_process_contact, contacts_list);

    return contacts_list;
}

/* costruisce il menu della sezione contatti */
Evas_Object* contactsview_make_menu(void)
{
    MokoWin* win = phone_win_get_mokowin();

    Evas_Object *bt_import = elm_button_add(win->win);
    elm_button_label_set(bt_import, _("Import from SIM"));

    evas_object_size_hint_weight_set(bt_import, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(bt_import, EVAS_HINT_FILL, 1.0);

    evas_object_smart_callback_add(bt_import, "clicked", _contacts_import_clicked, NULL);

    return bt_import;
}

void contactsview_reset_view(void)
{
    // se il db e' stato modificato...
    time_t new_time = get_modification_time(contactsdb_path);
    if (new_time != contactsdb_timestamp) {
        contactsdb_timestamp = new_time;

        elm_genlist_clear(contacts_list);
        // carica i contatti
        contactsdb_foreach_contact(
        #ifdef CONTACTSDB_SQLITE
        CONTACT_FIELD_NAME, CONTACT_DEFAULT_PHONE,
        #else
        NULL,
        #endif
        contactsview_process_contact, contacts_list);
    }

    Elm_Genlist_Item *item = elm_genlist_first_item_get(contacts_list);

    if (item)
        elm_genlist_item_show(item);
}
