#include "mop.h"

MODULE = Class::MOP::Mixin::AttributeCore   PACKAGE = Class::MOP::Mixin::AttributeCore

PROTOTYPES: DISABLE

BOOT:
    INSTALL_SIMPLE_READER(Mixin::AttributeCore, name);
