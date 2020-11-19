//
//  experimentTool.m
//  experimentTool
//

#import <Foundation/Foundation.h>
#import <Security/SecExperimentPriv.h>
#import <Security/SecTrustPriv.h>
#import <CoreFoundation/CFXPCBridge.h>
#import <xpc/xpc.h>

static NSString *kExperimentRunResultKeySkips = @"Skips";
static NSString *kExperimentRunResultKeyConfigurations = @"Configurations";
static NSString *kExperimentRunResultKeyRuns = @"Runs";

static NSDictionary<NSString *, NSObject *> *
run_experiment(const char *experiment_name, size_t num_runs, bool sampling_disabled)
{
    __block size_t runs = 0;
    __block size_t skips = 0;
    __block NSMutableArray<NSDictionary *> *configurations = [[NSMutableArray<NSDictionary *> alloc] init];
    for (size_t i = 0; i < num_runs; i++) {
        (void)sec_experiment_run_with_sampling_disabled(experiment_name, ^bool(const char *identifier, xpc_object_t experiment_config) {
            runs++;
            NSDictionary* configuration = (__bridge_transfer NSDictionary*)_CFXPCCreateCFObjectFromXPCObject(experiment_config);
            if (configuration != NULL) {
                [configurations addObject:configuration];
            }
            return true;
        }, ^(const char * _Nonnull identifier) {
            skips++;
        }, sampling_disabled);
    }

    return @{kExperimentRunResultKeyRuns: @(runs),
             kExperimentRunResultKeySkips: @(skips),
             kExperimentRunResultKeyConfigurations: configurations,
    };
}

static void
usage(const char *argv[])
{
    fprintf(stderr, "Usage: %s [-h] [-s] [-e <experiment>] [-n <number of runs>]\n", argv[0]);
}

int
main(int argc, const char *argv[])
{
    BOOL showUsage = NO;
    int arg = 0;
    char * const *gargv = (char * const *)argv;
    char *experiment_name = NULL;
    size_t num_runs = 0;
    bool sampling_disabled = true;
    bool update_asset = false;
    bool read_asset = false;
    while ((arg = getopt(argc, gargv, "e:n:sruh")) != -1) {
        switch (arg) {
            case 'e':
                free(experiment_name);      // Only the last instance of -e counts
                experiment_name = strdup(optarg);
                break;
            case 'n':
                num_runs = (size_t)atoi(optarg);
                break;
            case 's':
                sampling_disabled = false;
                break;
            case 'r':
                read_asset = true;
                break;
            case 'u':
                update_asset = true;
                break;
            case 'h':
                showUsage = YES;
                break;
            default:
                fprintf(stderr, "%s: FAILURE: unknown option \"%c\"\n", argv[0], arg);
                free(experiment_name);
                return -1;
        }
    }

    if (optind != argc) {
        fprintf(stderr, "%s: FAILURE: argument missing parameter \"%c\"\n", argv[0], arg);
        free(experiment_name);
        return -2;
    }

    if (showUsage) {
        usage(argv);
        free(experiment_name);
        return EXIT_SUCCESS;
    }

    uint64_t version = 0;
    if (update_asset) {
        CFErrorRef update_error = NULL;
        version = SecTrustOTASecExperimentGetUpdatedAsset(&update_error);
        if (update_error != NULL) {
            NSLog(@"Failed to fetch latest asset: %@", (__bridge NSError *)update_error);
            free(experiment_name);
            return -3;
        } else {
            NSLog(@"Fetched asset version: %zu", (size_t)version);
        }
    }

    if (read_asset) {
        CFErrorRef copy_error = NULL;
        NSDictionary *asset = CFBridgingRelease(SecTrustOTASecExperimentCopyAsset(&copy_error));
        if (copy_error != NULL) {
            NSLog(@"Failed to copy asset: %@", copy_error);
            free(experiment_name);
            return -4;
        } else {
            NSLog(@"Copied asset: %@", asset);
        }
    }

    if (num_runs > 0) {
        NSLog(@"Running %zu experiments with asset verison %llu", num_runs, version);
        NSDictionary<NSString *, NSObject *> *results = run_experiment(experiment_name, num_runs, sampling_disabled);
        [results enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSObject * _Nonnull obj, BOOL * _Nonnull stop) {
            NSLog(@"Experiment %@: %@", key, obj);
        }];
    } else {
        NSLog(@"Not running experiment.");
    }

    free(experiment_name);
    return EXIT_SUCCESS;
}
