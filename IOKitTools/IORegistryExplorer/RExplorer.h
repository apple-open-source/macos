#import <AppKit/AppKit.h>

#import <IOKit/IOKitLib.h>

#import "RBool.h"

@interface RExplorer : NSObject
{
    id			browser;
    id			window;
    id   		planeWindow;
    id			inspectorWindow;

    id			inspectorText;
    id			informationView;  // NSTextView ?

    id			splitView;
    id			propertiesOutlineView;

    id   		keyColumn;
    id			typeColumn;
    id   		valueColumn;

    id			updatePrefsMatrix;

    id			objectDescription;
    id			objectState;

    NSDictionary 	*currentSelectedItemDict;
    NSDictionary 	*aboutBoxOptions;

    int 		trackingRect;

    NSDictionary 	*registryDict;
    
    NSString		*currentLocation;
    int 		currentLevel;  // where are we at the current level ?
    id 			planeBrowser;
    const char		*currentPlane;

    NSMapTable		*_parentMap;
    NSMapTable		*_keyMap;

    int			autoUpdate;
    NSTimer		*updateTimer;
    mach_port_t		port;
    IONotificationPortRef notifyPort;
    Boolean		registryHasChanged;
    BOOL                dialogDisplayed;
    Boolean		registryHasQuieted;
}

- (void)changeLevel:(id)sender;

- (void)initializeRegistryDictionaryWithPlane:(const char *)plane;
- (NSDictionary *)dictForIterated:(io_registry_entry_t)passedEntry;
- (NSArray *)childArrayAtColumn:(int)column;
- (NSDictionary *) propertiesForRegEntry:(NSDictionary *)object;

- (void)dumpDictionaryToOutput:(id)sender;

- (void)displayAboutWindow:(id)sender;

- (void)switchRootPlane:(id)sender;
- (void)displayPlaneWindow:(id)sender;

- (void)initializeMapsForDictionary:(NSDictionary *)dict;

- (void)checkForUpdate:(NSTimer *)timer;

- (void)forceUpdate:(id)sender;

- (void)registryHasChanged;

- (void)goToPath:(NSString *)path;

- (NSArray *)searchResultsForText:(NSString *)text searchKeys:(BOOL)keys searchValues:(BOOL)values;

- (NSArray *)searchKeysResultsInDictionary:(NSDictionary *)dict forText:(NSString *)text passedPath:(NSString *)path;

- (void)updatePrefs:(id)sender;

@end
