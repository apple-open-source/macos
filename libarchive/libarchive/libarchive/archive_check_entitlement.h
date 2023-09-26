//
//  archive_check_entitlement.h
//
//  Created by Justin Vreeland on 6/13/23.
//  Copyright © 2023 Apple Inc. All rights reserved.
//
//

#ifndef archive_check_entitlement_h
#define archive_check_entitlement_h

#include <stdbool.h>

bool archive_allow_entitlement_format(const char *);
bool archive_allow_entitlement_filter(const char *);
void archive_entitlement_cleanup(void);

#endif /* archive_check_entitlement_h */
