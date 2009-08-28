/*
 *  odkerb_test.c
 *
 *  test harness for cross-realm auth
 *
 *  korver@apple.com
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

static void test_vsyslog(int priority, const char *fmt, va_list ap);
void
test_vsyslog(int priority, const char *fmt, va_list ap)
{
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    vsyslog(priority, fmt, ap);
}

/* override vsyslog call in odkerb.c:odkerb_log() */
#define vsyslog test_vsyslog
#include "../odkerb.c"
#undef vsyslog




#define test_assert(e)  \
    ((void) ((e) ? 0 : __test_assert(#e, __FILE__, __LINE__)))

void __test_assert(char *e, char *file, unsigned int line);
void
__test_assert(char *e, char *file, unsigned int line)
{
    fprintf(stderr, "%s:%u: failed test '%s'\n", file, line, e);
}

#define strsame(s1,s2) \
     ((s1)&&(s2)&&!strcmp((s1),(s2)))

void unit_test(void);
void
unit_test(void)
{
    CFStringRef good_id = CFSTR("korver@ODSNOWLEO.APPLE.COM");
    CFStringRef bogus_id = CFSTR("korver@KERBEROS.APPLE.COM");
    ODRecordRef record = NULL;
    ODNodeRef node = NULL;
    char  jid[1024];
    CFStringRef short_name = CFSTR("korver");

    test_assert(! odkerb_has_foreign_realm("foo@bar"));
    test_assert(odkerb_has_foreign_realm("foo@bar@baz"));

    test_assert(odkerb_copy_user_record_with_alt_security_identity(bogus_id, &record) != 0);
    test_assert(odkerb_copy_user_record_with_alt_security_identity(good_id, &record) == 0);
    test_assert(record != 0);
    test_assert(odkerb_get_im_handle_with_user_record(record, CFSTR(kIMTypeJABBER), CFSTR("ichatserver.apple.com"), short_name, jid, sizeof(jid)) == 0);
    test_assert(strsame(jid, "korver@ichatserver.apple.com"));
    record = 0;

    CFStringRef config_record_name = odkerb_create_config_record_name(good_id);
    test_assert(odkerb_copy_search_node_with_config_record_name(config_record_name, &node) == 0);
    test_assert(node != 0);
    test_assert(odkerb_copy_user_record_with_short_name(short_name, node, &record) == 0);
    test_assert(record != 0);
    test_assert(odkerb_get_im_handle_with_user_record(record, CFSTR(kIMTypeJABBER), CFSTR("ichatserver.apple.com"), short_name, jid, sizeof(jid)) == 0);
    test_assert(strsame(jid, "korver@ichatserver.apple.com"));
    record = 0;
    node = 0;

    test_assert(odkerb_get_im_handle("korver@ODSNOWLEO.APPLE.COM@SOMEWHERE.ORG", "ichatserver.apple.com", kIMTypeJABBER, jid, sizeof(jid)) == 0);
    test_assert(strsame(jid, "korver@ichatserver.apple.com"));

    test_assert(odkerb_get_im_handle("okay@NOWHERE.APPLE.COM", "ichatserver.apple.com", kIMTypeJABBER, jid, sizeof(jid)) == 0);
    test_assert(strsame(jid, "okay@ichatserver.apple.com"));

    test_assert(odkerb_get_im_handle("BOGUSBOGUS@ODSNOWLEO.APPLE.COM@SOMEWHERE.ORG", "ichatserver.apple.com", kIMTypeJABBER, jid, sizeof(jid)) != 0);
}

int
main(int argc, char *argv[])
{
    int i;
    char jid[512];
    int failure = 0;

    if (argc == 1) 
        unit_test();
    else {
        for (i = 1; i < argc; ++i) {
            if (odkerb_get_im_handle(argv[i], "ichatserver.apple.com", kIMTypeJABBER, jid, sizeof(jid)) == 0)
                fprintf(stderr, "%30s => %s\n", argv[i], jid);
            else {
                fprintf(stderr, "%s FAILED\n", argv[i]);
                ++failure;
            }
        }
    }

    return failure;
}

