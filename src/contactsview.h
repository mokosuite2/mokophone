#ifndef __CONTACTSVIEW_H
#define __CONTACTSVIEW_H

#include <mokosuite/ui/gui.h>
#include <Elementary.h>

Evas_Object* contactsview_make_section(void);
Evas_Object* contactsview_make_menu(void);

void contactsview_reset_view(void);

#endif  /* __CONTACTSVIEW_H */
