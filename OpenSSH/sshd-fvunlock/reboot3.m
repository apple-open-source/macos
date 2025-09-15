#import <reboot2.h>
#import "reboot3.h"

int reboot3_1_str(uint64_t flags, const char *arg1) {
    return reboot3(flags, arg1);
}
