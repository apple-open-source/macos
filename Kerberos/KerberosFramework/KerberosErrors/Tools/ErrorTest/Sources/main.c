#include <stdio.h>
#include <Kerberos/KerberosComErr.h>

int main (int argc, const char * argv[]) {
    // insert code here...
    printf("Hello, World!\n");
    printf ("This is an error message for #-108\n");
    printf ("Message: '%s'\n", error_message (-108));
    return 0;
}
