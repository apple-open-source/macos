//
//  exclavecore_support.c
//  libmalloc
//
//  Minirunner support for running darwintests in exclavecore
//

#include <stdio.h>
#include <darwintest.h>
#include <darwintest/accessors.h>
#include <stdlib.h>
#include <string.h>

#include <platform/platform.h>
#include <xrt/endpoint.h>


LIBLIBC_PLAT_EP(hello_endpoint, "HelloExclave");
static xrt_endpoint_t hello_xrt_endpoint;

static xrt_thread_t *main_thrd = NULL;

extern void minirunner_exclave(void);

static L4_MessageTag_t endpoint_thread(void *ctx, L4_MessageTag_t tag,
                                       L4_Word_t badge) {
    printf("[HELLO-C] ipcb %p tag %lx badge %lx\n", L4_IpcBuffer(), tag, badge);
    return L4_MessageTag(0, 0, L4_MessageTag_Label(tag) + 1, L4_False);
}

static int run_tests(void *arg) {
    dt_init_section_ptrs();
    /* Call the minirunner to start executing all tests */
    minirunner_exclave();
    return 0;
}

int xrt_init(void) {
    xrt_endpoint_config_t config = {
        .handler = endpoint_thread,
        .handler_ctx = NULL,
        .num_handlers = 1,
        .num_src_slots = 0,
        .num_dst_slots = 0,
    };
    if (hello_endpoint.ep_cap != L4_Nil) {
        printf("Spawning endpoint listener thread...\n");
        xrt_endpoint_create(&hello_xrt_endpoint, hello_endpoint.ep_cap, &config);
    } else {
        printf("Skipping endpoint listener thread due to nil cap!\n");
    }

    if (L4_Platform_Major(xrt__plat_get()) == L4_Platform_EVP) {
        printf("Spawning main thread...\n");
        xrt_thread_create(&main_thrd, run_tests, NULL);
    }
    return 0;
}

// TODO: Remove this when rdar://118895397 is fixed
T_HELPER_DECL(do_nothing, "Put the __dt_helper section into the output binary")
{

}
