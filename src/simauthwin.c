
#include <mokosuite/ui/gui.h>

#include "globals.h"
#include "simauthwin.h"


static void _delete(void* mokowin, Evas_Object* obj, void* event_info)
{
    g_debug("SIM auth delete callback");
    // TODO: comportamento?
    //mokowin_hide((MokoWin *)mokowin);
}

static void _number_pressed(void *data, Evas_Object* obj, const char* emission, const char* source)
{
    SimAuthWin *s = SIMAUTH_WIN(data);

    g_string_append(s->code, emission);

    char *fill = g_strnfill(s->code->len, '*');
    edje_object_part_text_set(MOKO_WIN(s)->layout_edje, "input_text", fill);
    g_free(fill);
}

static void _ok_keypad(void *data, Evas_Object *obj, void *event_info)
{
    SimAuthWin *s = SIMAUTH_WIN(data);

    g_debug("OK keypad: code_callback = %p", s->code_callback);
    if (s->code_callback != NULL)
        (s->code_callback)(s, s->code->str);
}

static void _delete_keypad(void *data, Evas_Object *obj, void *event_info)
{
    SimAuthWin *s = SIMAUTH_WIN(data);

    if (s->code->len > 0) {
        g_string_truncate(s->code, s->code->len - 1);
        _number_pressed(s, s->keypad_edje, "", "");
    }
}

void sim_auth_win_destroy(SimAuthWin *s)
{
    g_string_free(s->code, TRUE);
    mokoinwin_destroy(MOKO_INWIN(s->status));
    mokowin_destroy(MOKO_WIN(s));
}

void sim_auth_win_auth_error(SimAuthWin *s)
{
    const char *instr = NULL;

    switch (s->type) {
        case SIM_AUTH_STATUS_PIN_REQUIRED:
            instr = "PIN code is incorrect.";
            break;

        // TODO: altri case

        default:
            instr = "Code is incorrect.";
            break;
    }

    edje_object_part_text_set(MOKO_WIN(s)->layout_edje, "instruction", _(instr));
    g_string_erase(s->code, 0, -1);
    _number_pressed(s, s->keypad_edje, "", "");
}

SimAuthWin* sim_auth_win_new(SIMAuthStatus type)
{
    SimAuthWin *s = (SimAuthWin *) mokowin_sized_new("mokoauth", sizeof(SimAuthWin), TRUE);

    g_return_val_if_fail(s != NULL, NULL);

    elm_win_title_set(MOKO_WIN(s)->win, _("SIM auth"));
    elm_win_borderless_set(MOKO_WIN(s)->win, TRUE);

    mokowin_create_layout(MOKO_WIN(s), MOKOPHONE_DATADIR "/theme.edj", "phone/sim_auth");

    MOKO_WIN(s)->delete_callback = _delete;

    // sovrascrivi il callback di chiusura interno
    mokowin_delete_data_set(MOKO_WIN(s), s);

    // keypad
    // TODO passare widget keypad in libmokosuite-ui
    s->keypad = elm_layout_add(MOKO_WIN(s)->win);
    elm_layout_file_set(s->keypad, MOKOPHONE_DATADIR "/theme.edj", "common/keypad");
    s->keypad_edje = elm_layout_edje_get(s->keypad);
    edje_object_signal_callback_add(s->keypad_edje, "*", "input", _number_pressed, s);
    edje_object_signal_callback_add(s->keypad_edje, "0", "mouse_up", _number_pressed, s);

    edje_object_part_text_set(s->keypad_edje, "text2_desc", "ABC");
    edje_object_part_text_set(s->keypad_edje, "text3_desc", "DEF");
    edje_object_part_text_set(s->keypad_edje, "text4_desc", "GHI");
    edje_object_part_text_set(s->keypad_edje, "text5_desc", "JKL");
    edje_object_part_text_set(s->keypad_edje, "text6_desc", "MNO");
    edje_object_part_text_set(s->keypad_edje, "text7_desc", "PQRS");
    edje_object_part_text_set(s->keypad_edje, "text8_desc", "TUV");
    edje_object_part_text_set(s->keypad_edje, "text9_desc", "WXYZ");

    // tasto ok
    Evas_Object* bt_ok = elm_button_add(MOKO_WIN(s)->win);
    elm_button_label_set(bt_ok, _("OK"));
    evas_object_smart_callback_add(bt_ok, "clicked", _ok_keypad, s);

    elm_layout_content_set(MOKO_WIN(s)->layout, "button_ok", bt_ok);
    evas_object_show(bt_ok);

    // tasto cancella
    Evas_Object* bt_del = elm_button_add(MOKO_WIN(s)->win);
    elm_button_label_set(bt_del, _("Delete"));
    evas_object_smart_callback_add(bt_del, "clicked", _delete_keypad, s);

    elm_layout_content_set(MOKO_WIN(s)->layout, "button_delete", bt_del);
    evas_object_show(bt_del);

    const char *instr = NULL;
    switch (type) {
        case SIM_AUTH_STATUS_PIN_REQUIRED:
            instr = "Enter PIN to unlock SIM card.";
            break;

        // TODO: altri case

        default:
            instr = "Enter code.";
            break;
    }

    edje_object_part_text_set(MOKO_WIN(s)->layout_edje, "instruction", _(instr));

    elm_layout_content_set(MOKO_WIN(s)->layout, "keypad", s->keypad);
    evas_object_show(s->keypad);

    // altra roba
    s->code_callback = NULL;
    s->type = type;
    s->code = g_string_new("");
    s->status = moko_popup_status_new(MOKO_WIN(s), NULL);

    return s;
}
