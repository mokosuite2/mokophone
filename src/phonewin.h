#ifndef __PHONEWIN_H
#define __PHONEWIN_H

#include <glib.h>
#include <mokosuite/ui/gui.h>

enum {
    SECTION_PHONE = 0,
    SECTION_LOG,
    SECTION_CONTACTS,

    NUM_SECTIONS
};

bool phone_win_call_internal(const char *peer, bool *hide);

void phone_win_ussd_reply(int mode, const char* message);

void phone_win_goto_section(int section, gboolean force);

void phone_win_activate(int section, gboolean callwin);
void phone_win_hide(void);

MokoWin* phone_win_get_mokowin(void);

void phone_win_init(void);


#endif  /* __PHONEWIN_H */
