#include "mop.h"

MODULE = Class::MOP::Method   PACKAGE = Class::MOP::Method

PROTOTYPES: DISABLE

BOOT:
    INSTALL_SIMPLE_READER(Method, name);
    INSTALL_SIMPLE_READER(Method, package_name);
    INSTALL_SIMPLE_READER(Method, body);
