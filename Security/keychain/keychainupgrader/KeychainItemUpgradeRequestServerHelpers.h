
#ifndef KeychainItemUpgradeRequestServerHelpers_h
#define KeychainItemUpgradeRequestServerHelpers_h

#include <stdbool.h>

__BEGIN_DECLS
bool KeychainItemUpgradeRequestServerIsEnabled(void);
void KeychainItemUpgradeRequestServerSetEnabled(bool enabled);
void KeychainItemUpgradeRequestServerInitialize(void);
__END_DECLS

#endif /* KeychainItemUpgradeRequestServerHelpers_h */
