#ifndef __CALLWIN_H
#define __CALLWIN_H

#include "callblock.h"

#include <glib.h>
#include <freesmartphone-glib/ogsmd/call.h>
#include <mokosuite/utils/utils.h>
#include <libnotify/notify.h>

#define CALL_NOTIFICATION_SOUND         "call_notification_sound"
#define CALL_NOTIFICATION_VIBRATION     "call_notification_vibration"
#define CALL_NOTIFICATION_RINGTONE      "call_notification_ringtone"

int phone_call_win_num_calls(void);

void phone_call_win_outgoing_call(const char* peer);

void phone_call_win_call_remove(PhoneCallBlock* call);
void phone_call_win_call_status(int id, CallStatus status, GHashTable* properties);

void phone_call_win_activate(void);
void phone_call_win_hide(void);

void phone_call_win_init(void);

extern NotifyNotification* call_notification;

#endif  /* __CALLWIN_H */
