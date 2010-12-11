#ifndef __CALLBLOCK_H
#define __CALLBLOCK_H

#include <glib.h>
#include <freesmartphone-glib/ogsmd/call.h>
#include <Evas.h>

#include <mokosuite/utils/utils.h>
#include <mokosuite/ui/gui.h>

struct _PhoneCallBlock {
    /* finestra padre */
    MokoWin* parent;

    /* ID chiamata */
    int id;

    /* ID notifica panel */
    int notification_id;

    /* chiamata accettata? (solo per uso dialer) */
    bool accepted;

    /* chiamata risposta? (per uso ogsmd) */
    bool answered;

    /* numero del chiamate/chiamato */
    char* peer;

    /* stato corrente chiamata */
    int status;

    /* stato iniziale chiamata -- usato per determinare le cose da inserire nel database */
    int old_status;

    /* timer chiamata */
    GTimer* timer;

    /* durata chiamata (usato solo per appoggio) */
    unsigned long duration;

    /* timestamp inizio chiamata */
    unsigned long timestamp;

    /* widget bubble contenente la chiamata */
    Evas_Object* widget;

    /* layout del contenuto del blocco */
    Evas_Object* layout;
    Evas_Object* layout_edje;

    /* pulsante reject (solo incoming) */
    Evas_Object* button_reject;

    /* pulsanti hold e release */
    Evas_Object* button_hold;
    Evas_Object* button_release;

    ScrollCallbackData* data_freeze;
    ScrollCallbackData* data_unfreeze;
};

typedef struct _PhoneCallBlock PhoneCallBlock;

#define PHONE_CALL_BLOCK(x)     ((PhoneCallBlock*)x)

void phone_call_block_update_hold(PhoneCallBlock* call);

void phone_call_block_call_status(PhoneCallBlock* call, const CallStatus status, GHashTable* properties);

void phone_call_block_destroy(PhoneCallBlock* call);
PhoneCallBlock* phone_call_block_new(MokoWin* parent, const char* peer, int id, bool outgoing);

#endif  /* __CALLBLOCK_H */
