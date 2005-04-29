#include <stdio.h>
#include <pthread.h>
#include <Kerberos/Kerberos.h>

#include "ErrorTestErrors.h"

typedef struct thread_context {
    u_int32_t count;
    errcode_t errors[64];
    char *thread_name;
} *thread_context_t;

struct thread_context thread_context1 = { 5,
    { 0, memFullErr, EINVAL, klCantContactServerErr, KRB5KRB_AP_ERR_BAD_INTEGRITY, 200000 },
    "thread 1" };

struct thread_context thread_context2 = { 6,
    { errTest1, errTest2, errTest3, errTest4, errTest5, errTest5 + 1 },
    "thread 2" };

void *my_thread_routine (void *context)
{
    thread_context_t thread_context = (thread_context_t) context;
    char *thread_name = thread_context->thread_name;
    int i;

    printf ("%s: Entering %s()...\n", thread_name, __FUNCTION__);
    for (i = 0; i < thread_context->count; i++) {
        printf ("%s: i = %d\n", thread_name, i);

        errcode_t err = thread_context->errors[i];
        const char *message = error_message (err);
        printf ("%s: error_message (%ld): message '%s' at %lx\n", thread_name, err, message, (long) message);
        sleep (1);        
        printf ("%s: error_message (%ld): message '%s' at %lx\n", thread_name, err, message, (long) message);
        sleep (1);
    }  
    printf ("%s: Exiting %s()...\n", thread_name, __FUNCTION__);
    
    return 0;
}

int main (int argc, const char * argv[]) 
{
    int err = 0;
    pthread_t thread1, thread2;
    
    add_error_table (&et_test_error_table);
    
    pthread_create (&thread1, NULL, my_thread_routine, &thread_context1);
    pthread_create (&thread2, NULL, my_thread_routine, &thread_context2);

    pthread_join (thread1, NULL);
    pthread_join (thread2, NULL);
    return 0;
}
