#ifndef X_ENV_H
#define X_ENV_H

#include "m_env.h"
#include "defs.h"
#include "sconf.h"

extern env_h std_env;
status_e initenv(void);
status_e setup_environ(struct service_config *scp,struct service_config *def);

#endif
