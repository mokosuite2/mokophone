#ifndef __GSM_H
#define __GSM_H

#include <mokosuite/utils/utils.h>
#include <mokosuite/utils/remote-config-service.h>

bool gsm_is_ready(void);
bool gsm_can_call(void);

void gsm_online(void);
void gsm_offline(void);

void gsm_init(RemoteConfigService *config);

#endif  /* __GSM_H */
