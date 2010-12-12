
#include "globals.h"
#include "gsm.h"

#include <mokosuite/utils/notify.h>
#include <mokosuite/ui/gui.h>
#include <mokosuite/pim/callsdb.h>
#include <freesmartphone-glib/ogsmd/network.h>
#include <Ecore_X.h>

#include "phonewin.h"
#include "callwin.h"
#include "simauthwin.h"
#include "logview.h"
#include "contactsview.h"

#define LONG_PRESS_TIME     750

/* finestra principale */
static MokoWin* win = NULL;

MokoPopupStatus* waiting_ussd = NULL;
extern DBusGProxy* panel_notifications;
static NotifyNotification* ussd_notification = NULL;
static guint ussd_count = 0;

/* menu_hover delle sezioni */
typedef Evas_Object* (*MenuGenerator)(void);
static MenuGenerator menus[NUM_SECTIONS];

/* pagine delle sezioni */
static Evas_Object* pages[NUM_SECTIONS];
gboolean first_switch = TRUE;

/* -- sezione telefono -- */
static Evas_Object* obj_pager;
static Evas_Object* obj_keypad;
static Evas_Object* keypad_edje;
static Evas_Object* dialer_edje;

static GString* number;
static guint plus_timer = 0;
static guint delete_timer = 0;

/* -- sezione log -- SPOSTATO IN logview.c */

/* -- sezione contatti -- SPOSTATO IN contactsview.c */

static Evas_Object* menu_phone_section(void);

static void _delete(void* mokowin, Evas_Object* obj, void* event_info)
{
    if (waiting_ussd) {
        mokoinwin_destroy(MOKO_INWIN(waiting_ussd));
        waiting_ussd = NULL;
    }

    else
        mokowin_hide((MokoWin *)mokowin);
}

/* -- sezione telefono -- */

static void _number_pressed(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    g_string_append(number, emission);

    edje_object_part_text_set(dialer_edje, "text_number", number->str);

    if (number->len > 0) {
        // abilita il menu' locale
        if (!win->current_menu)
            mokowin_menu_set(win, (menus[SECTION_PHONE])());
    }

    else {
        mokowin_menu_set(win, NULL);
    }
}

static gboolean _plus_pressed(gpointer data)
{
    plus_timer = 0;
    _number_pressed(NULL, keypad_edje, "+", "");
    return FALSE;
}

static void _zero_mouse_down(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    if (!plus_timer) plus_timer = g_timeout_add(LONG_PRESS_TIME, _plus_pressed, NULL);
}

static void _zero_mouse_up(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    if (plus_timer > 0) {
        g_source_remove(plus_timer);
        plus_timer = 0;
        _number_pressed(NULL, keypad_edje, "0", "");
    }
}

static void _dial_pressed(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    if (number->len > 0) {
        bool done = FALSE;
        bool hide = FALSE;

        done = phone_win_call_internal(number->str, &hide);

        if (done) {
            // reset numero digitato e nascondi finestra
            g_string_erase(number, 0, -1);
            _number_pressed(NULL, keypad_edje, "", "");

            if (hide) mokowin_hide(win);
        }
    }
}

static gboolean _delete_pressed(gpointer data)
{
    delete_timer = 0;
    if (number->len > 0) {
        g_string_truncate(number, 0);
        _number_pressed(NULL, keypad_edje, "", "");
    }

    return FALSE;
}

static void _delete_mouse_down(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    if (!delete_timer) delete_timer = g_timeout_add(LONG_PRESS_TIME, _delete_pressed, NULL);
}

static void _delete_mouse_up(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    if (delete_timer > 0) {
        g_source_remove(delete_timer);
        delete_timer = 0;
        g_string_truncate(number, number->len - 1);
        _number_pressed(NULL, keypad_edje, "", "");
    }
}

static void _tab_change(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    phone_win_goto_section((int)data, FALSE);
}

/* costruisce la sezione phone */
static Evas_Object* make_dialer(void)
{
    Evas_Object *dialer = elm_layout_add(win->win);
    evas_object_size_hint_weight_set(dialer, 1.0, 1.0);

    elm_layout_file_set(dialer, MOKOPHONE_DATADIR "/theme.edj", "mokosuite/phone/dialer");

    dialer_edje = elm_layout_edje_get(dialer);
    edje_object_signal_callback_add(dialer_edje, "mouse,clicked,1", "bg_number", _dial_pressed, NULL);
    edje_object_signal_callback_add(dialer_edje, "mouse,down,1", "bg_delete", _delete_mouse_down, NULL);
    edje_object_signal_callback_add(dialer_edje, "mouse,up,1", "bg_delete", _delete_mouse_up, NULL);

    evas_object_show(dialer);

    // keypad
    obj_keypad = elm_layout_add(win->win);
    elm_layout_file_set(obj_keypad, MOKOPHONE_DATADIR "/theme.edj", "mokosuite/common/dialpad");

    // keypad signals
    keypad_edje = elm_layout_edje_get(obj_keypad);

    edje_object_part_text_set(keypad_edje, "text2_desc", "ABC");
    edje_object_part_text_set(keypad_edje, "text3_desc", "DEF");
    edje_object_part_text_set(keypad_edje, "text4_desc", "GHI");
    edje_object_part_text_set(keypad_edje, "text5_desc", "JKL");
    edje_object_part_text_set(keypad_edje, "text6_desc", "MNO");
    edje_object_part_text_set(keypad_edje, "text7_desc", "PQRS");
    edje_object_part_text_set(keypad_edje, "text8_desc", "TUV");
    edje_object_part_text_set(keypad_edje, "text9_desc", "WXYZ");

    edje_object_signal_callback_add(keypad_edje, "*", "input", _number_pressed, NULL);
    edje_object_signal_callback_add(keypad_edje, "0", "mouse_down", _zero_mouse_down, NULL);
    edje_object_signal_callback_add(keypad_edje, "0", "mouse_up", _zero_mouse_up, NULL);

    evas_object_show(obj_keypad);
    elm_layout_content_set(dialer, "keypad", obj_keypad);

    return dialer;
}

static Evas_Object* menu_phone_section(void)
{
    Evas_Object *m = elm_table_add(win->win);
    elm_table_homogenous_set(m, TRUE);

    evas_object_size_hint_weight_set(m, 1.0, 0.0);
    evas_object_size_hint_align_set(m, -1.0, 1.0);

    /* pulsante aggiungi ai contatti */
    Evas_Object *bt_contacts = elm_button_add(win->win);
    elm_button_label_set(bt_contacts, _("Add to contacts"));

    evas_object_size_hint_weight_set(bt_contacts, 1.0, 0.0);
    evas_object_size_hint_align_set(bt_contacts, -1.0, 1.0);

    evas_object_show(bt_contacts);
    elm_table_pack(m, bt_contacts, 0, 0, 1, 1);

    /* pulsante invia messaggio */
    Evas_Object *bt_send_msg = elm_button_add(win->win);
    elm_button_label_set(bt_send_msg, _("Send message"));

    evas_object_size_hint_weight_set(bt_send_msg, 1.0, 0.0);
    evas_object_size_hint_align_set(bt_send_msg, -1.0, 1.0);

    evas_object_show(bt_send_msg);
    elm_table_pack(m, bt_send_msg, 1, 0, 1, 1);

    return m;
}

static Evas_Object* make_pager(Evas_Object* pages_buf[])
{
    Evas_Object *p = elm_pager_add(win->win);
    evas_object_size_hint_weight_set(p, 1.0, 1.0);
    evas_object_size_hint_align_set(p, -1.0, -1.0);
    elm_object_style_set(p, "custom");
    evas_object_show(p);

    /* sezione telefono */
    pages_buf[SECTION_PHONE] = make_dialer();
    elm_pager_content_push(p, pages[SECTION_PHONE]);

    /* sezione contatti (deve essere chiamato prima del log per il lookup) */
    pages_buf[SECTION_CONTACTS] = contactsview_make_section();
    elm_pager_content_push(p, pages[SECTION_CONTACTS]);

    /* sezione log */
    pages_buf[SECTION_LOG] = logview_make_section();
    elm_pager_content_push(p, pages[SECTION_LOG]);

    return p;
}

static gboolean _goto_section(gpointer section)
{
    first_switch = FALSE;
    elm_pager_content_promote(obj_pager, pages[(int)section]);
    return FALSE;
}

void phone_win_goto_section(int section, gboolean force)
{
    g_return_if_fail(win != NULL);

    g_debug("Going to section %d", section);

    if (force) {
        // forza l'highlight del pulsante se richiesto
        char *src = g_strdup_printf("button%d", section + 1);

        edje_object_signal_emit(win->layout_edje, "mouse,down,1", src);
        edje_object_signal_emit(win->layout_edje, "mouse,up,1", src);

        g_free(src);
    }

    // promuovi la pagina del telefono
    // FIXME workaround per un bug di edje... :|
    if (first_switch)
        g_idle_add(_goto_section, (gpointer)section);
    else
        elm_pager_content_promote(obj_pager, pages[section]);

    // sezione log -- resetta vista
    if (section == SECTION_LOG)
        logview_reset_view();

    // sezione contatti -- resetta vista
    if (section == SECTION_CONTACTS)
        contactsview_reset_view();

    // cancella il vecchio menu manualmente
    if (win->current_menu)
        mokowin_menu_set(win, NULL);

    // imposta il menu hover
    if (section == SECTION_PHONE) {
        _number_pressed(NULL, keypad_edje, "", "");
    } else {
        mokowin_menu_set(win, (menus[section])());

        //FIXME vedi sopra -- if (section == SECTION_LOG)
        //    phone_log_init();
    }

    // remove notification anyway
    if (ussd_notification) {
        notify_notification_close(ussd_notification, NULL);
        g_object_unref(ussd_notification);
        ussd_notification = NULL;
        ussd_count = 0;
    }
}

// giusto per sicurezza...
static void _ussd_request(GError *error, gpointer data)
{
    if (error) {
        g_debug("Error: %s", error->message);
        if (waiting_ussd) {
            mokoinwin_destroy(MOKO_INWIN(waiting_ussd));
            waiting_ussd = NULL;
        }
    }
}

bool phone_win_call_internal(const char *peer, bool *hide)
{
    gboolean ready = gsm_can_call();
    gboolean _hide = FALSE;

    if (peer == NULL || !strlen(peer)) return FALSE;

    if (ready) {
        if (strlen(peer) < 3 || peer[0] == '*' || peer[0] == '#') {
            waiting_ussd = moko_popup_status_new(win, _("Sending USSD request"));

            ogsmd_network_send_ussd_request(peer, _ussd_request, NULL);
        } else {
            phone_call_win_outgoing_call(peer);
            _hide = TRUE;
        }

    } else {
        g_debug("Not registered to network");
        moko_popup_alert_new(win, _("Not registered to network."));
    }

    if (hide != NULL)
        *hide = _hide;

    return ready;
}

static void _ussd_action(NotifyNotification *notification, char *action, gpointer user_data)
{
    g_debug("ActionInvoked! (%s)", action);
    if (action != NULL && !strcmp(action, "ussd-show"))
        // activate phone section
        phone_win_activate(SECTION_PHONE, FALSE);
}

void phone_win_ussd_reply(int mode, const char* message)
{
    // distruggi eventuale inwin status
    if (waiting_ussd) {
        mokoinwin_destroy(MOKO_INWIN(waiting_ussd));
        waiting_ussd = NULL;
    }

    if (message == NULL || (message != NULL && !strlen(message)))
        message = _("USSD request sent.");

    g_debug("USSD reply (%d) \"%s\"", mode, message);

    // se la finestra del telefono e' fuori fuoco, mandare il messaggio al notifier
    Ecore_X_Window xwin = elm_win_xwindow_get(win->win);

    if (!ecore_x_window_visible_get(xwin) || ecore_x_window_focus_get() != xwin) {
        g_debug("Notifying USSD response to notification daemon");

        // notify to notification daemon
        ussd_count++;
        if (!ussd_notification) {
            char* msg;
            if (ussd_count == 1) msg = g_strdup(_("New USSD message"));
            else msg = g_strdup_printf(_("%d new USSD message"), ussd_count);

            ussd_notification = mokosuite_notification_new("ussd-message",
                msg, message, NULL, 0);
            notify_notification_add_action(ussd_notification, "ussd-show", "Show", _ussd_action, NULL, NULL);
            notify_notification_show(ussd_notification, NULL);

            g_free(msg);
        }
    }

    moko_popup_alert_new(win, message);
}

void phone_win_activate(int section, gboolean callwin)
{
    g_return_if_fail(win != NULL);

    if (phone_call_win_num_calls() > 0 && callwin == TRUE) {
        phone_call_win_activate();
    } else {
        phone_win_goto_section(section, TRUE);
        mokowin_activate(win);
    }
}

void phone_win_hide(void)
{
    g_return_if_fail(win != NULL);

    mokowin_hide(win);
}

MokoWin* phone_win_get_mokowin(void)
{
    return win;
}

void phone_win_init(void)
{
    // overlay per l'animazione del pager
    elm_theme_overlay_add(NULL, "elm/pager/base/custom");

    win = mokowin_new("mokophone", TRUE);
    if (win == NULL) {
        g_error("Cannot create main window. Exiting");
        return;
    }

    elm_win_title_set(win->win, _("Phone"));
    elm_win_borderless_set(win->win, TRUE);

    mokowin_create_layout(win, MOKOPHONE_DATADIR "/theme.edj", "mokosuite/phone/main");
    mokowin_menu_enable(win);

    win->delete_callback = _delete;

    number = g_string_new("");

    menus[SECTION_PHONE] = &menu_phone_section;
    menus[SECTION_LOG] = &logview_make_menu;
    menus[SECTION_CONTACTS] = &contactsview_make_menu;

    edje_object_part_text_set(win->layout_edje, "text1", _("Dialer"));
    edje_object_part_text_set(win->layout_edje, "text2", _("Call log"));
    edje_object_part_text_set(win->layout_edje, "text3", _("Contacts"));

    char *source;

    source = g_strdup_printf("%d", SECTION_PHONE + 1);
    edje_object_signal_callback_add(win->layout_edje, source, "tab_change", _tab_change, (void*)SECTION_PHONE);
    g_free(source);

    source = g_strdup_printf("%d", SECTION_LOG + 1);
    edje_object_signal_callback_add(win->layout_edje, source, "tab_change", _tab_change, (void*)SECTION_LOG);
    g_free(source);

    source = g_strdup_printf("%d", SECTION_CONTACTS + 1);
    edje_object_signal_callback_add(win->layout_edje, source, "tab_change", _tab_change, (void*)SECTION_CONTACTS);
    g_free(source);

    obj_pager = make_pager(pages);
    elm_layout_content_set(win->layout, "pager", obj_pager);

    // TEST
    evas_object_resize(win->win, 480, 640);

    // inizia con la sezione telefono
    phone_win_goto_section(SECTION_PHONE, TRUE);
}
