#include "httpd.h"
#include "http_config.h"
extern module dav_module;
module * stub_function(void)
{
    return &dav_module;
}
