/* RSearch.h created by epeyton on Fri 14-Jan-2000 */

#import <AppKit/AppKit.h>

@interface RSearch : NSObject
{
    id			resultsTable;
    id			searchKeysCheckBox;
    id   		searchValuesCheckBox;
    id   		searchTextField;
    id   		searchWindow;

    id			explorer;

    NSArray 		*resultsArray;
}

- (void)search:(id)sender;
- (void)goTo:(id)sender;
- (void)goToNextResult:(id)sender;
- (void)goToPreviousResult:(id)sender;

- (void)displaySearchWindow:(id)sender;

@end
