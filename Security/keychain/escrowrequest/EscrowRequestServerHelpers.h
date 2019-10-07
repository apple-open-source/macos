
#ifndef EscrowRequestServerHelpers_h
#define EscrowRequestServerHelpers_h

#include <stdbool.h>
bool EscrowRequestServerIsEnabled(void);
void EscrowRequestServerSetEnabled(bool enabled);
void EscrowRequestServerInitialize(void);

#endif /* EscrowRequestServerHelpers_h */
