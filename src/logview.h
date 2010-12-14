#ifndef __LOGVIEW_H
#define __LOGVIEW_H

#include <mokosuite/ui/gui.h>
#include <mokosuite/pim/callsdb.h>
#include <Elementary.h>

Evas_Object* logview_make_section(void);
Evas_Object* logview_make_menu(void);
void logview_reset_view(void);

#endif  /* __LOGVIEW_H */
