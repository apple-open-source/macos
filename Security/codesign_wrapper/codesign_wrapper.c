/*
   cc -g -O0 -Wall -Werror -Wmost -stabs -o codesign_wrapper codesign_wrapper.c -framework CoreFoundation
 */

#include <paths.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <getopt.h>
#include <stdbool.h>
#include <limits.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>

/* for CMS */
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/oidsalg.h>
#include <Security/cmspriv.h>

#include <Security/SecCMS.h>

#include <Security/SecPolicy.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>

/* entitlement whitelist checking */
#include <MISEntitlement.h>

#define DEBUG_ASSERT_PRODUCTION_CODE 0
#include <AssertMacros.h>

#include "codesign_wrapper.h"
#include "codesign.h"

extern bool do_verify(const char *path, CFArrayRef certificates);

static char * codesign_binary = "/usr/bin/codesign";
static char * processing_path = "/var/tmp/signingbox";
static char * processing_file = "codesign_wrapper";
static char * processing_prefix = NULL;
static char * auditing_postfix = "_auditing.plist";
static char * audition_plist_path = NULL;
static char * entitlements_plist_path = NULL;
static char * entitlements_postfix = "_entitlements.plist";


#define CODESIGN_WRAPPER_VERSION "0.7.10"
#define log(format, args...)    \
    fprintf(stderr, "codesign_wrapper-" CODESIGN_WRAPPER_VERSION ": " format "\n", ##args);
#define cflog(format, args...) do { \
CFStringRef logstr = CFStringCreateWithFormat(NULL, NULL, CFSTR(format), ##args);\
if (logstr) { CFShow(logstr); CFRelease(logstr); } \
} while(0); \

const char *_root_ca_name = ANCHOR;

static pid_t kill_child = -1;
static void child_timeout(int sig)
{
    if (kill_child != -1) {
        kill(kill_child, sig);
        kill_child = -1;
    }
}

static void
close_all_fd(void *arg __unused)
/* close down any files that might have been open at this point
   but make sure 0, 1 and 2 are set to /dev/null so they don't
   get reused */
{
    int maxDescriptors = getdtablesize ();
    int i;

    int devnull = open(_PATH_DEVNULL, O_RDWR, 0);

    if (devnull >= 0) for (i = 0; i < 3; ++i)
        dup2(devnull, i);

    for (i = 3; i < maxDescriptors; ++i)
        close (i);
}


static pid_t
fork_child(void (*pre_exec)(void *arg), void *pre_exec_arg,
        const char * const argv[])
{
    unsigned delay = 1, maxDelay = 60;
    for (;;) {
        pid_t pid;
        switch (pid = fork()) {
            case -1: /* fork failed */
                switch (errno) {
                    case EINTR:
                        continue; /* no problem */
                    case EAGAIN:
                        if (delay < maxDelay) {
                            sleep(delay);
                            delay *= 2;
                            continue;
                        }
                        /* fall through */
                    default:
                        perror("fork");
                        return -1;
                }
                assert(-1); /* unreached */

            case 0: /* child */
                if (pre_exec)
                    pre_exec(pre_exec_arg);
                execv(argv[0], (char * const *)argv);
                perror("execv");
                _exit(1);

            default: /* parent */
                return pid;
                break;
        }
        break;
    }
    return -1;
}


static int
fork_child_timeout(void (*pre_exec)(), char *pre_exec_arg,
        const char * const argv[], int timeout)
{
    int exit_status = -1;
    pid_t child_pid = fork_child(pre_exec, pre_exec_arg, argv);
    if (timeout) {
        kill_child = child_pid;
        alarm(timeout);
    }
    while (1) {
        int err = wait4(child_pid, &exit_status, 0, NULL);
        if (err == -1) {
            perror("wait4");
            if (errno == EINTR)
                continue;
        }
        if (err == child_pid) {
            if (WIFSIGNALED(exit_status)) {
                log("child %d received signal %d", child_pid, WTERMSIG(exit_status));
                kill(child_pid, SIGHUP);
                return -2;
            }
            if (WIFEXITED(exit_status))
                return WEXITSTATUS(exit_status);
            return -1;
        }
    }
}


static void
dup_io(int arg[])
{
    dup2(arg[0], arg[1]);
    close(arg[0]);
}


static int
fork_child_timeout_output(int child_fd, int *parent_fd, const char * const argv[], int timeout)
{
    int output[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, output))
        return -1;
    fcntl(output[1], F_SETFD, 1); /* close in child */
    int redirect_child[] = { output[0], child_fd };
    int err = fork_child_timeout(dup_io, (void*)redirect_child, argv, timeout);
    if (!err) {
        close(output[0]); /* close the child side in the parent */
        *parent_fd = output[1];
    }
    return err;
}


static void
pass_signal_to_children(int sig)
{
    signal(sig, SIG_DFL);
    kill(0, sig);
}


static int
mk_temp_dir(const char *path)
{
    char *pos = NULL, *tmp_path = strdup(path);
    if (!path) return -1;
    pos = index(tmp_path, '/');
    if (!pos) return -1;
    while ((pos = index(pos + 1, '/'))) {
        *pos = '\0';
        if ((0 != mkdir(tmp_path, 0755)) &&
                errno != EEXIST)
            return -1;
        *pos = '/';
    }
    if ((0 != mkdir(tmp_path, 0755)) &&
            errno != EEXIST)
        return -1;
    return 0;
}


static int
lock_file(const char *lock_file_prefix, const char *lock_filename)
{
    int err = -1;
    pid_t pid;
    char *tempfile = NULL;
    do {
        if (!asprintf(&tempfile, "%s.%d", lock_file_prefix, getpid()))
            break;
        FILE *temp = fopen(tempfile, "w");
        if (temp == NULL)
            break;
        if (fprintf(temp, "%d\n", getpid()) <= 0)
            break;
        fclose(temp);
        if(!link(tempfile, lock_filename)) {
            unlink(tempfile);
            err = 0;
            break;
        }
        FILE* lock = fopen(lock_filename, "r");
        if (lock == NULL)
            break;
        if (fscanf(lock, "%d\n", &pid) <= 0)
            break;
        if (kill(pid, 0)) {
            if (!unlink(lock_filename) &&
                    !link(tempfile, lock_filename)) {
                unlink(tempfile);
                err = 0;
                break;
            }
        }
    } while(0);
    unlink(tempfile);
    if (tempfile)
        free(tempfile);

    return err;
}


static ssize_t
read_fd(int fd, void **buffer)
{
    int err = -1;
    size_t capacity = 1024;
    char * data = malloc(capacity);
    size_t size = 0;
    while (1) {
        int bytes_left = capacity - size;
        int bytes_read = read(fd, data + size, bytes_left);
        if (bytes_read >= 0) {
            size += bytes_read;
            if (capacity == size) {
                capacity *= 2;
                data = realloc(data, capacity);
                if (!data) {
                    err = -1;
                    break;
                }
                continue;
            }
            err = 0;
        } else
            err = -1;
        break;
    }
    if (0 == size) {
        if (data)
            free(data);
        return err;
    }

    *buffer = data;
    return size;
}

enum { CSMAGIC_EMBEDDED_ENTITLEMENTS = 0xfade7171 };

typedef struct {
    uint32_t type;
    uint32_t offset;
} cs_blob_index;

static CFDataRef
extract_entitlements_blob(const uint8_t *data, size_t length)
{
    CFDataRef entitlements = NULL;
    cs_blob_index *csbi = (cs_blob_index *)data;

    require(data && length, out);
    require(csbi->type == ntohl(CSMAGIC_EMBEDDED_ENTITLEMENTS), out);
    require(length == ntohl(csbi->offset), out);
    entitlements = CFDataCreate(kCFAllocatorDefault,
        (uint8_t*)(data + sizeof(cs_blob_index)),
        (CFIndex)(length - sizeof(cs_blob_index)));
out:
    return entitlements;
}

static CFDataRef
build_entitlements_blob(const uint8_t *data, size_t length)
{
    cs_blob_index csbi = { htonl(CSMAGIC_EMBEDDED_ENTITLEMENTS),
        htonl(length+sizeof(csbi)) };
    CFMutableDataRef blob = CFDataCreateMutable(kCFAllocatorDefault, sizeof(csbi)+length);
    if (data) {
        CFDataAppendBytes(blob, (uint8_t*)&csbi, sizeof(csbi));
        CFDataAppendBytes(blob, data, length);
    }
    return blob;
}

static CFMutableDictionaryRef
dump_auditing_info(const char *path)
{
    int exit_status;
    CFMutableDictionaryRef dict = NULL;
    void *requirements = NULL;
    ssize_t requirements_size = 0;
    void *entitlements = NULL;
    ssize_t entitlements_size = 0;

    do {
        const char * const extract_requirements[] =
        { codesign_binary, "--display", "-v", "-v", path, NULL };
        int requirements_fd;
        if ((exit_status = fork_child_timeout_output(STDERR_FILENO, &requirements_fd,
                        extract_requirements, 0))) {
            fprintf(stderr, "failed to extract requirements data: %d\n", exit_status);
            break;
        }

        requirements_size = read_fd(requirements_fd, &requirements);
        if (requirements_size == -1)
            break;
        close(requirements_fd);

    } while(0);

    do {
        const char * const extract_entitlements[] =
        { codesign_binary, "--display", "--entitlements", "-", path, NULL };
        int entitlements_fd;
        if ((exit_status = fork_child_timeout_output(STDOUT_FILENO, &entitlements_fd,
                        extract_entitlements, 0))) {
            fprintf(stderr, "failed to extract entitlements: %d\n", exit_status);
            break;
        }

        entitlements_size = read_fd(entitlements_fd, &entitlements);
        if (entitlements_size == -1)
            break;
        close(entitlements_fd);

    } while(0);

    do {
        dict = CFDictionaryCreateMutable(
                kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks);

        if (requirements && requirements_size) {
            CFDataRef req = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                requirements, requirements_size, kCFAllocatorMalloc);
            CFDictionarySetValue(dict, CFSTR("Requirements"), req);
            CFRelease(req);
        }

        if (entitlements && entitlements_size) {
            CFDataRef ent = extract_entitlements_blob(entitlements, entitlements_size);
            free(entitlements);
            require(ent, out);
            CFPropertyListRef entitlements_dict =
                CFPropertyListCreateWithData(kCFAllocatorDefault,
                ent, kCFPropertyListImmutable, NULL, NULL);
            CFRelease(ent);
            require(entitlements_dict, out);
            CFDictionarySetValue(dict, CFSTR("Entitlements"), entitlements_dict);
            CFRelease(entitlements_dict);
        }
    } while (0);

    return dict;
out:
    return NULL;
}

static int
write_data(const char *path, CFDataRef data)
{
    int fd = open(path, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    ssize_t length = CFDataGetLength(data);
    if (fd < 0)
        return -1;
    int bytes_written = write(fd, CFDataGetBytePtr(data), length);
    close(fd);
    CFRelease(data);
    if (bytes_written != length) {
        fprintf(stderr, "failed to write auditing info to %s\n", path);
        unlink(path);
        return -1;
    }
    return 0;

}

static int
write_auditing_data(const char *path, CFMutableDictionaryRef info)
{
    CFTypeRef entitlements = CFDictionaryGetValue(info, CFSTR("Entitlements"));
    if (entitlements) {
        CFDataRef entitlements_xml = CFPropertyListCreateXMLData(kCFAllocatorDefault, entitlements);
        if (!entitlements_xml)
            return -1;
        CFDictionarySetValue(info, CFSTR("Entitlements"), entitlements_xml);
        CFRelease(entitlements_xml);
    }

    CFDataRef plist = CFPropertyListCreateXMLData(kCFAllocatorDefault, info);
    if (!plist)
        return -1;

    return write_data(path, plist); /* consumes plist */
}

static int
write_filtered_entitlements(const char *path, CFDictionaryRef info)
{
    CFDataRef plist = CFPropertyListCreateXMLData(kCFAllocatorDefault, info);
    if (!plist)
        return -1;
    CFDataRef entitlements_blob =
        build_entitlements_blob(CFDataGetBytePtr(plist), CFDataGetLength(plist));
    CFRelease(plist);
    if (!entitlements_blob)
        return -1;
    return write_data(path, entitlements_blob); /* consumes entitlements_blob */

}

static CFDataRef
cfdata_read_file(const char *filename)
{
    int data_file = open(filename, O_RDONLY);
    if (data_file == -1)
        return NULL;
    void *data = NULL;
    ssize_t size = read_fd(data_file, &data);
    if (size > 0)
        return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                data, size, kCFAllocatorMalloc);

    return NULL;
}

static CFDictionaryRef
load_profile(const char *profile_path)
{
    SecCmsMessageRef cmsg = NULL;
    CFDictionaryRef entitlements = NULL;
    CFArrayRef certificates = NULL;
    CFDictionaryRef profile = NULL;

    CFDataRef message = cfdata_read_file(profile_path);
    require(message, out);
    SecAsn1Item encoded_message = { CFDataGetLength(message),
        (uint8_t*)CFDataGetBytePtr(message) };
    require_noerr(SecCmsMessageDecode(&encoded_message,
                NULL, NULL, NULL, NULL, NULL, NULL, &cmsg), out);

    /* expected to be a signed data message at the top level */
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd;
    require(cinfo = SecCmsMessageContentLevel(cmsg, 0), out);
    require(SecCmsContentInfoGetContentTypeTag(cinfo) ==
            SEC_OID_PKCS7_SIGNED_DATA, out);
    require(sigd = (SecCmsSignedDataRef)SecCmsContentInfoGetContent(cinfo), out);

    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    policy = SecPolicyCreateBasicX509();
    int nsigners = SecCmsSignedDataSignerInfoCount(sigd);
    require(nsigners == 1, out);
    require_noerr(SecCmsSignedDataVerifySignerInfo(sigd, 0, NULL, policy, &trust), out);
    SecCertificateRef apple_ca_cert = NULL;
    CFArrayRef apple_ca_cert_anchors = NULL;
    require(apple_ca_cert = SecCertificateCreateWithBytes(NULL, _profile_anchor, sizeof(_profile_anchor)), out);
    require(apple_ca_cert_anchors = CFArrayCreate(kCFAllocatorDefault, (const void **)&apple_ca_cert, 1, NULL), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, apple_ca_cert_anchors), out);
    log("using %s for profile evaluation", _root_ca_name);
    SecTrustResultType trust_result;
    require_noerr(SecTrustEvaluate(trust, &trust_result), out);
#if WWDR
    /* doesn't mean much, but I don't have the root */
    require(trust_result == kSecTrustResultRecoverableTrustFailure, out);
#else
    require(trust_result == kSecTrustResultUnspecified, out);
#endif
    CFRelease(apple_ca_cert_anchors);

    // FIXME require proper intermediate and leaf certs
    // require_noerr(SecCertificateCopyCommonName(SecCertificateRef certificate, CFStringRef *commonName);
    CFRelease(trust);

    CFRelease(policy);
    SecCmsSignerInfoRef sinfo = SecCmsSignedDataGetSignerInfo(sigd, 0);
    require(sinfo, out);
    CFStringRef commonname = SecCmsSignerInfoGetSignerCommonName(sinfo);
    require(commonname, out);
#if WWDR
    require(CFEqual(CFSTR("Alpha Config Profile Signing Certificate"), commonname), out);
#else
    require(CFEqual(CFSTR("Apple iPhone OS Provisioning Profile Signing"), commonname) ||
            CFEqual(CFSTR("TEST Apple iPhone OS Provisioning Profile Signing TEST"), commonname), out);
#endif
    CFRelease(commonname);

    /* attached CMS */
    const SecAsn1Item *content = SecCmsMessageGetContent(cmsg);
    require(content && content->Length && content->Data, out);

    CFDataRef attached_contents = CFDataCreate(kCFAllocatorDefault,
            content->Data, content->Length);
    CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault,
            attached_contents, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(attached_contents);
    require(plist && CFGetTypeID(plist) == CFDictionaryGetTypeID(), out);

    CFTypeRef profile_certificates = CFDictionaryGetValue(plist, CFSTR("DeveloperCertificates"));
    if (profile_certificates && CFGetTypeID(profile_certificates) == CFArrayGetTypeID())
    {
        certificates = CFArrayCreateCopy(kCFAllocatorDefault, profile_certificates);
#if 0
        CFIndex i, cert_count = CFArrayGetCount(certificates);
        for (i = 0; i < cert_count; i++) {
            SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, CFArrayGetValueAtIndex(certificates, i));
            CFShow(cert);
            CFRelease(cert);
        }
#endif
    }

    CFTypeRef profile_entitlements = CFDictionaryGetValue(plist, CFSTR("Entitlements"));
    if (profile_entitlements && CFGetTypeID(profile_entitlements) == CFDictionaryGetTypeID())
    {
        entitlements = CFDictionaryCreateCopy(kCFAllocatorDefault,
                (CFDictionaryRef)profile_entitlements);
    }
    CFRelease(plist);

out:
    if (cmsg) SecCmsMessageDestroy(cmsg);
    if (entitlements && certificates) {
        const void *keys[] = { CFSTR("Entitlements"), CFSTR("Certificates") };
        const void *vals[] = { entitlements, certificates };
        profile = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    if (entitlements) CFRelease(entitlements);
    if (certificates) CFRelease(certificates);
    return profile;
}

typedef struct {
    CFDictionaryRef whitelist;
    CFMutableDictionaryRef filtered_list;
    bool allowed_entitlements;
} filter_whitelist_ctx;

static void
filter_entitlement(const void *key, const void *value,
        filter_whitelist_ctx *ctx)
{
    /* filter out get-task-allow, no error */
    if (CFEqual(key, CFSTR("get-task-allow")))
        return;

    /* whitelist data protection entitlement, otherwise validate */
    if (!CFEqual(key, CFSTR("DataProtectionClass")) && 
            !CFEqual(key, CFSTR("data-protection-class")) &&
            !MISEntitlementDictionaryAllowsEntitlementValue(ctx->whitelist, key, value)) {
        ctx->allowed_entitlements = false;
        cflog("Illegal entitlement key/value pair: %@, %@", key, value);
        return;
    }

    if (ctx->filtered_list)
        CFDictionarySetValue(ctx->filtered_list, key, value);
}

static bool
filter_entitlements(CFDictionaryRef whitelist, CFDictionaryRef entitlements,
    CFMutableDictionaryRef filtered_entitlements)
{
    if (!entitlements)
        return true;

    filter_whitelist_ctx ctx = { whitelist, filtered_entitlements, true };
    CFDictionaryApplyFunction(entitlements,
            (CFDictionaryApplierFunction)filter_entitlement, &ctx);
    return ctx.allowed_entitlements;
}

static SecCertificateRef
cms_verify_signer(CFDataRef message, CFDataRef detached)
{
    SecCertificateRef signer_cert = NULL;
    require(message, out);

    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    policy = SecPolicyCreateBasicX509();

    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd = NULL;

    SecAsn1Item encoded_message = { CFDataGetLength(message), (uint8_t*)CFDataGetBytePtr(message) };
    require_noerr(SecCmsMessageDecode(&encoded_message, NULL, NULL, NULL, NULL, NULL, NULL, &cmsg),
        out);
    /* expected to be a signed data message at the top level */
    require(cinfo = SecCmsMessageContentLevel(cmsg, 0), out);
    require(SecCmsContentInfoGetContentTypeTag(cinfo) == SEC_OID_PKCS7_SIGNED_DATA, out);
    require(sigd = (SecCmsSignedDataRef)SecCmsContentInfoGetContent(cinfo), out);

    if (detached) {
        require(!SecCmsSignedDataHasDigests(sigd), out);
        SECAlgorithmID **digestalgs = SecCmsSignedDataGetDigestAlgs(sigd);
        SecCmsDigestContextRef digcx = SecCmsDigestContextStartMultiple(digestalgs);
        SecCmsDigestContextUpdate(digcx, CFDataGetBytePtr(detached), CFDataGetLength(detached));
        SecCmsSignedDataSetDigestContext(sigd, digcx);
        SecCmsDigestContextDestroy(digcx);
    }

    int nsigners = SecCmsSignedDataSignerInfoCount(sigd);
    require_quiet(nsigners == 1, out);
    require_noerr_string(SecCmsSignedDataVerifySignerInfo(sigd, 0, NULL, policy, &trust), out, "bad signature");

    signer_cert = SecTrustGetCertificateAtIndex(trust, 0);
    CFRetain(signer_cert);
    CFRelease(policy);
    CFRelease(trust);

out:
    return signer_cert;

}

static bool
cms_verify(CFDataRef message, CFDataRef detached, CFArrayRef certificates)
{
    bool result = false;
    SecCertificateRef signer_cert = cms_verify_signer(message, detached);
    require(signer_cert, out);
    if (certificates) {
        CFDataRef cert_cfdata = SecCertificateCopyData(signer_cert);
        CFRange all_certs = CFRangeMake(0, CFArrayGetCount(certificates));
        result = CFArrayContainsValue(certificates, all_certs, cert_cfdata);
        CFRelease(cert_cfdata);
    } else {
        CFArrayRef commonNames = SecCertificateCopyCommonNames(signer_cert);
        require(CFArrayGetCount(commonNames) == 1, out);
        CFStringRef commonName = (CFStringRef)CFArrayGetValueAtIndex(commonNames, 0);
        require(commonName, out);
        result = CFEqual(CFSTR("Apple iPhone OS Application Signing"), commonName)
            || CFEqual(CFSTR("TEST Apple iPhone OS Application Signing TEST"), commonName);
        CFRelease(commonNames);
    }

    if (!result)
        fprintf(stderr, "Disallowed signer\n");

out:
    if (signer_cert) CFRelease(signer_cert);
    return result;

}


static bool
verify_code_signatures(CFArrayRef code_signatures, CFArrayRef certificates)
{
    require(code_signatures, out);
    CFIndex i, signature_count = CFArrayGetCount(code_signatures);

    /* Each slice can have their own entitlements and be properly signed
       but codesign(1) picks the first when listing and smashes that one
       down when re-signing */
    CFDataRef first_entitlement_hash = NULL;
    for (i = 0; i < signature_count; i++) {
        CFDictionaryRef code_signature = CFArrayGetValueAtIndex(code_signatures, i);

        CFDataRef signature = CFDictionaryGetValue(code_signature, CFSTR("SignedData"));
        require(signature, out);
        CFDataRef code_directory = CFDictionaryGetValue(code_signature, CFSTR("CodeDirectory"));
        require(code_directory, out);
        CFDataRef entitlements = CFDictionaryGetValue(code_signature, CFSTR("Entitlements"));
        CFDataRef entitlements_hash = CFDictionaryGetValue(code_signature, CFSTR("EntitlementsHash"));
        CFDataRef entitlements_cdhash = CFDictionaryGetValue(code_signature, CFSTR("EntitlementsCDHash"));
        require(entitlements, out);
        require(entitlements_hash, out);
        require(entitlements_cdhash, out);
        require(CFEqual(entitlements_hash, entitlements_cdhash), out);

        if (!first_entitlement_hash)
            first_entitlement_hash = entitlements_hash;
        else
            require(entitlements_hash && CFEqual(first_entitlement_hash, entitlements_hash), out);

        /* was the application signed by a certificate in the profile */
        require(cms_verify(signature, code_directory, certificates), out);
    }
    return true;
out:
    return false;
}


static void
init()
{
    signal(SIGHUP, pass_signal_to_children);
    signal(SIGINT, pass_signal_to_children);
    signal(SIGTERM, pass_signal_to_children);
    //signal(SIGCHLD, SIG_IGN);
    signal(SIGALRM, child_timeout);

    const char *codesign_binary_env = getenv("CODESIGN");
    if (codesign_binary_env)
        codesign_binary = strdup(codesign_binary_env);

    const char *processing_path_env = getenv("PROCESS_PATH");
    if (processing_path_env)
        processing_path = strdup(processing_path_env);

    processing_prefix = calloc(1, strlen(processing_path) +
            strlen(processing_file) + 1/*'/'*/ + 1/*'\0'*/);
    strcat(processing_prefix, processing_path);
    strcat(processing_prefix, "/");
    strcat(processing_prefix, processing_file);

    audition_plist_path = calloc(1, strlen(processing_prefix) +
            strlen(auditing_postfix) + 1);
    strcat(audition_plist_path, processing_prefix);
    strcat(audition_plist_path, auditing_postfix);

    entitlements_plist_path = calloc(1, strlen(processing_prefix) +
            strlen(entitlements_postfix) + 1);
    strcat(entitlements_plist_path, processing_prefix);
    strcat(entitlements_plist_path, entitlements_postfix);

}


const struct option options[] = {
    { "sign", required_argument, NULL, 's' },
    { "entitlements", required_argument, NULL, 'z' },
    { "no-profile", no_argument, NULL, 'Z' },
    { "verify", no_argument, NULL, 'V' }, /* map to V to let verbose v pass */
    { "timeout", required_argument, NULL, 't' },
    {}
};

struct securityd *gSecurityd;
void securityd_init();
CFArrayRef SecAccessGroupsGetCurrent(void);

CFArrayRef SecAccessGroupsGetCurrent(void) {
    return NULL;
}

OSStatus ServerCommandSendReceive(uint32_t id, CFTypeRef in, CFTypeRef *out);
OSStatus ServerCommandSendReceive(uint32_t id, CFTypeRef in, CFTypeRef *out)
{
    return -1;
}



#ifndef UNIT_TESTING
int
main(int argc, char *argv[])
{
    int err = 0;

    int ch;
    bool sign_op = false, noprofile = false,
        verify_op = false;
    int timeout = 180;

    securityd_init();

    while ((ch = getopt_long(argc, argv, "fvr:s:R:", options, NULL)) != -1)
    {
        switch (ch) {
            case 's': sign_op = true; break;
            case 'z': { log("codesign_wrapper reserves the entitlements option for itself");
                        exit(1); /* XXX load entitlements from optarg */
                        break; }
            case 'Z': noprofile = true; break;
            case 'V': verify_op = true; break;
            case 't': timeout = atoi(optarg); break;
        }
    }
    int arg_index_files = optind;
    if ((!sign_op && !verify_op) || arg_index_files == argc) {
        log("not a signing/verify operation, or no file to sign given");
        return 1; /* short circuit to codesign binary: not signing, no files */
    }
    if (arg_index_files + 1 != argc) {
        log("cannot sign more than one file in an operation");
        return 1; /* we don't do more than one file at a time, so we can rejigger */
    }

    init();
    if (mk_temp_dir(processing_path)) {
        log("failed to create directory %s", processing_path);
        return 1;
    }

    CFMutableDictionaryRef auditing_info =
        dump_auditing_info(argv[arg_index_files]);
    if (!auditing_info) {
        log("failed to extract auditing_info from %s", argv[arg_index_files]);
        return 1;
    }

    /* load up entitlements requested */
    CFDictionaryRef entitlements_requested =
        CFDictionaryGetValue(auditing_info, CFSTR("Entitlements"));
    require_string(entitlements_requested, out, "At least need an application-identifier entitlements");
    CFMutableDictionaryRef allowable_entitlements = NULL;

    if (noprofile) {
        /* XXX if (verify_op) require it to be store signed */
        if (verify_op) {
            /* load the code signature */
            CFArrayRef code_signatures =
                load_code_signatures(argv[arg_index_files]);
            require(code_signatures, out);
            require(verify_code_signatures(code_signatures, NULL), out);
            CFRelease(code_signatures);
        }

        if (sign_op) {
            /* do the same checks, pass signed in entitlements along for audit */
            require(CFDictionaryGetValue(entitlements_requested, 
                                         CFSTR("application-identifier")), out);

             CFDictionarySetValue(auditing_info, CFSTR("Entitlements"),
                                  entitlements_requested);

             allowable_entitlements = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, entitlements_requested);

             /* For the 2-pass signing, where the app is signed first, then encrypted
                and then resigned, we need to by pass the initial validation, so we
                allow signing without checking the entitlements to the profile. */
#if 0
            log("You shouldn't want to sign without a profile.");
            exit(1);
#endif
        }

    } else {
        /* load up the profile */
        char profile_path[_POSIX_PATH_MAX] = {};
        snprintf(profile_path, sizeof(profile_path), "%s/embedded.mobileprovision", argv[arg_index_files]);
        CFDictionaryRef profile = load_profile(profile_path);
        require_action(profile, out, log("Failed to load provision profile from: %s", profile_path));
        CFDictionaryRef entitlements_whitelist = CFDictionaryGetValue(profile, CFSTR("Entitlements"));
        require(entitlements_whitelist, out);
        CFArrayRef certificates = CFDictionaryGetValue(profile, CFSTR("Certificates"));
        require(certificates, out);

        if (sign_op)
            require_noerr(unlink(profile_path), out);

        /* only allow identifiers whitelisted by profile */
        allowable_entitlements = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require(allowable_entitlements, out);
        require(filter_entitlements(entitlements_whitelist,
                    entitlements_requested, allowable_entitlements), out);

        /* must have valid application-identifier */
        require(CFDictionaryGetValue(allowable_entitlements,
            CFSTR("application-identifier")), out);

        CFDictionarySetValue(auditing_info, CFSTR("Entitlements"),
            allowable_entitlements);

        if (verify_op) {
            /* load the code signature */
            CFArrayRef code_signatures =
                load_code_signatures(argv[arg_index_files]);
            require(code_signatures, out);
            require(verify_code_signatures(code_signatures, certificates), out);
            CFRelease(code_signatures);
        }
    }

    char *lock_filename = NULL;

    if (sign_op) {
        if (!asprintf(&lock_filename, "%s.lock", processing_prefix)) {
            log("failed to alloc %s.lock", processing_prefix);
            return 1;
        }

        while (lock_file(processing_prefix, lock_filename)) {
            log("waiting for lock");
            sleep(1);
        }

        err = write_auditing_data(audition_plist_path, auditing_info);

        if (!err && allowable_entitlements) {
            err |= write_filtered_entitlements(entitlements_plist_path, allowable_entitlements);
        }

        if (err)
            log("failed to write auditing data");
    }

    if (!err) {
        char *orig_args[argc+1+2];
        /* size_t argv_size = argc * sizeof(*argv); args = malloc(argv_size); */
        memcpy(orig_args, argv, (argc-1) * sizeof(*argv));

        int arg = 0, argo = 0;
        while (arg < argc - 1) {
            if (strcmp("--no-profile", orig_args[arg]) &&
                strncmp("--timeout", orig_args[arg], strlen("--timeout"))) {
                orig_args[argo] = argv[arg];
                argo++;
            }
            arg++;
        }
        if (entitlements_requested && allowable_entitlements) {
            orig_args[argo++] = "--entitlements";
            orig_args[argo++] = entitlements_plist_path;
        }
        orig_args[argo++] = argv[arg_index_files];
        orig_args[argo++] = NULL;
        orig_args[0] = codesign_binary;
#if DEBUG
        log("Caling codesign with the following args:");
        int ix;
        for(ix = 0; ix <= argc; ix++)
            log("   %s", orig_args[ix] ? orig_args[ix] : "NULL");
#endif
        err = fork_child_timeout(NULL, NULL, (const char * const *)orig_args, timeout);
    }

    if (sign_op) {
        unlink(audition_plist_path);
        unlink(entitlements_plist_path);

        free(audition_plist_path);
        free(entitlements_plist_path);

        if (err == -2) {
            log("executing codesign(1) timed out");
            const char * const kill_tokens[] = { "/usr/bin/killall", "Ingrian", NULL };
            fork_child_timeout(close_all_fd, NULL, kill_tokens, 0);
            const char * const load_tokens[] = { "/usr/bin/killall", "-USR2", "securityd", NULL };
            fork_child_timeout(close_all_fd, NULL, load_tokens, 0);
        }

        unlink(lock_filename);
        free(lock_filename);

        if (err == -2) {
            sleep(10);
            log("delayed exit with timeout return value now we've tried to reload tokens");
            return 2;
        }
    }

    if (!err)
        return 0;
    else
        log("failed to execute codesign(1)");
out:
    return 1;
}
#endif /* UNIT_TESTING */

/* vim: set et : set sw=4 : set ts=4 : set sts=4 : */
