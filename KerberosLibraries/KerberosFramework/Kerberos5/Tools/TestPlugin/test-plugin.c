#include <stdio.h>

#if defined(TEST_PLUGIN) | defined(TEST_BUNDLE)

char test_string[] = "test_string for " PLUGIN_NAME;

void test_function (void);

void test_function (void) 
{
    printf ("Executing test_function for %s.\n", PLUGIN_NAME);
}

#else

#include "k5-int.h"

const char * const filebases[] = { "test-plugin", "nonexistent", NULL };
const char * const directories[] = { "/tmp", "/tmp/plugins", NULL };

int main (void) {
    int err = 0;
    struct errinfo ep;
    struct plugin_dir_handle dir_handle = { NULL };
    void **test_strings = NULL;
    void (**test_functions)(void) = NULL;
    int i;
    
    if (!err) {
        err = krb5int_open_plugin_dirs (directories, filebases, 
                                        &dir_handle, &ep);
    }
    
    if (!err) {
        err = krb5int_get_plugin_dir_data (&dir_handle, "test_string",
                                           &test_strings, &ep);
    }
    
    if (!err) {
        for (i = 0; test_strings[i] != NULL; i++) {
            printf ("Plugin #%i has test string '%s'\n", i, (char *)test_strings[i]);
        }
    }
    
    if (!err) {
        err = krb5int_get_plugin_dir_func (&dir_handle, "test_function",
                                           &test_functions, &ep);
    }
    
    if (!err) {
        for (i = 0; test_functions[i] != NULL; i++) {
            printf ("Calling test function for plugin #%i...\n", i);
            test_functions[i]();
        }
    }
    
    if (test_functions       != NULL) { krb5int_free_plugin_dir_func (test_functions); }
    if (test_strings         != NULL) { krb5int_free_plugin_dir_data (test_strings); }
    if (PLUGIN_DIR_OPEN(&dir_handle)) { krb5int_close_plugin_dirs (&dir_handle); }
    
    return err ? 1 : 0;
}

#endif
