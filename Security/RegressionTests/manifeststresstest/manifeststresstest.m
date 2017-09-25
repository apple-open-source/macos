//
//  manifeststresstest.m
//  Security
//
//  Created by Ben Williamson on 6/1/17.
//
//

#import <Foundation/Foundation.h>
#import <stdlib.h>

#import "Monkey.h"
#import "Config.h"
#import "Keychain.h"
#import "mark.h"
#import <Security/SecItemPriv.h>

static const char *usage_message =
"Usage: mainfeststresstest <command> [options...]\n"
"\n"
"Commands:\n"
"\n"
" reset    Delete all items in the access group. Do this before starting the test.\n"
"\n"
" monkey   Randomly add, update and delete items for a while. Then delete those items.\n"
"        --seed <n>     Seed the random number generator\n"
"        --steps <n>    Stop after n random actions. Default 1000.\n"
"        --maxitems <n> Limit number of items created to n. Default 20.\n"
"        --nocleanup    Leave the items in place when finished.\n"
"        --dryrun       Print actions to stdout but don't actually touch the keychain.\n"
"        --name         Specialize items names (to isolate and avoid interfering changes)\n"
"        --view         Keychain syncing view name. If not, specified \"Engram\" is used\n"
"\n"
" mark <id> [view] Write some items containing the string <id>, to mark the known finishing\n"
"                  state for this device. The view argument takes a keychain syncing view name;\n"
"                  If not specified, the view is \"Engram\".\n"
"\n"
" unmark <id>  Delete the items that make up the mark for this id.\n"
"\n"
" update <id>  Update the items that make up the mark for this id.\n"
"\n"
" verify <id>... Check that the access group contains only the marks for the\n"
"                given <id> list, corresponding to all the devices being finished.\n"
"                If given an empty id list this checks the access group is empty.\n"
"                Exits with nonzero status if verification fails.\n"
"\n"
" verify_update <id>... Check that the access group contains only the updated marks for\n"
"                       the given <id> list, corresponding to all the devices being\n"
"                       finished. If given an empty id list this checks the access group\n"
"                       is empty. Exits with nonzero status if verification fails.\n"
"\n"
"Example:\n"
"\n"
" manifeststresstest reset\n"
" manifeststresstest monkey --seed 12345 --steps 1000 --maxitems 20\n"
" manifeststresstest mark foo\n"
" manifeststresstest verify foo bar baz\n"
"\n"
" One device should run reset to clear the contents of the access group before the test\n"
" begins. Then all devices should run monkey for a while. When each device is finished\n"
" monkeying it should set a pattern with an id that uniquely identifies that device.\n"
" When all devices are finished, one device can verify all the patterns by running the\n"
" verify command with the ids of all of the devices, as it expects to see the patterns\n"
" written by all devices, and no other items.\n"
"\n"
;

static void usage_exit(void)
{
    printf("%s", usage_message);
    exit(1);
}

int main(int argc, const char ** argv)
{
    @autoreleasepool {
        Keychain *keychain = [[Keychain alloc] init];
        
        NSArray<NSString *> *args = [[NSProcessInfo processInfo] arguments];
        if ([args count] < 2) {
            usage_exit();
        }
        NSString *verb = args[1];

        if ([verb isEqualToString:@"reset"]) {
            printf("Reseting\n");
            NSLog(@"reset - deleteAllItems");
            [keychain deleteAllItems];

        } else if ([verb isEqualToString:@"monkey"]) {
            BOOL dryrun = NO;
            BOOL cleanup = YES;
            unsigned steps = 1000;
            Config *config = [[Config alloc] init];
            config.maxItems = 20;
            config.distinctNames = 40;
            config.distinctValues = 10;
            config.addItemWeight = 20;
            config.deleteItemWeight = 10;
            config.updateNameWeight = 10;
            config.updateDataWeight = 10;
            config.updateNameAndDataWeight = 10;
            config.view = (__bridge NSString *)kSecAttrViewHintEngram;
            
            NSUInteger i = 2;
            while (i < [args count]) {
                NSString *opt = args[i++];
                if ([opt isEqualToString:@"--seed"]) {
                    if (i >= [args count]) {
                        printf("error: --seed needs a value\n");
                        exit(1);
                    }
                    unsigned seed = (unsigned)[args[i++] integerValue];
                    NSLog(@"Seeding with %d", seed);
                    srandom(seed);
                } else if ([opt isEqualToString:@"--steps"]) {
                    if (i >= [args count]) {
                        printf("error: --steps needs a value\n");
                        exit(1);
                    }
                    steps = (unsigned)[args[i++] integerValue];
                } else if ([opt isEqualToString:@"--maxitems"]) {
                    if (i >= [args count]) {
                        printf("error: --maxitems needs a value\n");
                        exit(1);
                    }
                    config.maxItems = (unsigned)[args[i++] integerValue];
                } else if ([opt isEqualToString:@"--nocleanup"]) {
                    cleanup = NO;
                } else if ([opt isEqualToString:@"--dryrun"]) {
                    dryrun = YES;
                } else if ([opt isEqualToString:@"--name"]) {
                    if (i >= [args count]) {
                        printf("error: --name needs a value\n");
                        exit(1);
                    }
                    config.name = args[i++];
                } else if ([opt isEqualToString:@"--view"]) {
                    if (i >= [args count]) {
                        printf("error: --view needs a value\n");
                        exit(1);
                    }
                    config.view = args[i++];
                } else {
                    printf("Unrecognised argument %s\n", [opt UTF8String]);
                    exit(1);
                }
            }
            NSLog(@"steps: %d", steps);
            NSLog(@"maxitems: %d", config.maxItems);
            NSLog(@"cleanup: %s", cleanup ? "yes" : "no");
            NSLog(@"dryrun: %s", dryrun ? "yes" : "no");

            Monkey *monkey = [[Monkey alloc] initWithConfig:config];
            if (!dryrun) {
                monkey.keychain = keychain;
            }
            
            while (monkey.step < steps) {
                [monkey advanceOneStep];
            }
            
            if (cleanup) {
                [monkey cleanup];
            }

        } else if ([verb isEqualToString:@"mark"]) {
            if ([args count] < 3) {
                printf("mark command needs an identifier\n");
                exit(1);
            }
            NSString *ident = args[2];
            NSString *view = (__bridge NSString*)kSecAttrViewHintEngram;
            if ([args count] == 4) {
                view = args[3];
            }
            NSLog(@"Writing mark %@", ident);
            writeMark(ident, view);

        } else if ([verb isEqualToString:@"unmark"]) {
            if ([args count] < 3) {
                printf("unmark command needs an identifier\n");
                exit(1);
            }
            NSString *ident = args[2];
            NSLog(@"Deleting mark %@", ident);
            deleteMark(ident);
            
        } else if ([verb isEqualToString:@"verify"]) {
            NSRange range;
            range.location = 2;
            range.length = args.count - 2;
            NSArray<NSString*> *idents = [args subarrayWithRange:range];
            NSLog(@"Verifying, idents = %@", idents);
            if (!verifyMarks(idents)) {
                NSLog(@"Exiting nonzero status");
                exit(1);
            }

        } else if ([verb isEqualToString:@"update"]) {
            if ([args count] < 3) {
                printf("unmark command needs an identifier\n");
                exit(1);
            }
            NSString *ident = args[2];
            NSLog(@"Updating mark %@", ident);
            updateMark(ident);

        } else if ([verb isEqualToString:@"verify_update"]) {
            NSRange range;
            range.location = 2;
            range.length = args.count - 2;
            NSArray<NSString*> *idents = [args subarrayWithRange:range];
            NSLog(@"Verifying, idents = %@", idents);
            if (!verifyUpdateMarks(idents)) {
                NSLog(@"Exiting nonzero status");
                exit(1);
            }

        } else {
            usage_exit();
        }
        NSLog(@"Done.");
    }
}
