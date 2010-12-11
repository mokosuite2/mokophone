#ifndef __LOGENTRY_H
#define __LOGENTRY_H

#include <mokosuite/pim/callsdb.h>

void logentry_new(CallEntry* c);

void logentry_init(void (*choose_callback)(CallEntry*, int));

#endif  /* __LOGENTRY_H */
