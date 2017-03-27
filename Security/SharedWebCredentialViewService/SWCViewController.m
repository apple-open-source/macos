//
//  SWCViewController.m
//  SharedWebCredentialViewService
//
//  Copyright (c) 2014 Apple Inc. All Rights Reserved.
//

#import <Foundation/NSXPCConnection.h>
#import "SWCViewController.h"
#import <UIKit/UIViewController_Private.h>
#import <UIKit/UIFont_Private.h>

#import <UIKit/UIAlertController_Private.h>
#import <UIKit/UITableViewCell_Private.h>

#include <bsm/libbsm.h>
#include <ipc/securityd_client.h>

#include "SharedWebCredential/swcagent_client.h"

#import "SWCViewController.h"

const NSString* SWC_PASSWORD_KEY = @"spwd";
const NSString* SWC_ACCOUNT_KEY  = @"acct";
const NSString* SWC_SERVER_KEY   = @"srvr";

//
// SWCDictionaryAdditions
//

@interface NSDictionary (SWCDictionaryAdditions)
- (NSComparisonResult) compareCredentialDictionaryAscending:(NSDictionary *)other;
@end

@implementation NSDictionary (SWCDictionaryAdditions)
- (NSComparisonResult)compareCredentialDictionaryAscending:(NSDictionary *)other
{
    NSComparisonResult result;
    NSString *str1 = [self objectForKey:SWC_ACCOUNT_KEY], *str2 = [other objectForKey:SWC_ACCOUNT_KEY];
    if (!str1) str1 = @"";
    if (!str2) str2 = @"";
    
    // primary sort by account name
    result = [str1 localizedCaseInsensitiveCompare:str2];
    if (result == NSOrderedSame) {
        // secondary sort by domain name
        NSString *str3 = [self objectForKey:SWC_SERVER_KEY], *str4 = [other objectForKey:SWC_SERVER_KEY];
        if (!str3) str3 = @"";
        if (!str4) str4 = @"";
        
        result = [str3 localizedCaseInsensitiveCompare:str4];
    }
    
    return result;
}
@end


//
// SWCItemCell
//
@interface SWCItemCell : UITableViewCell
{
    NSDictionary *_dict;
    BOOL _isTicked;
    UIView *_bottomLine;
    UIView *_bottomLineSelected;
    UIView *_topLine;
    UIView *_topLineSelected;
    BOOL _showSeparator;
    BOOL _showTopSeparator;
}

- (id)initWithDictionary:(NSDictionary *)dict;
@property (nonatomic, readonly) id userInfo;
@property (nonatomic, assign) BOOL showSeparator;
@end

@implementation SWCItemCell

@synthesize showSeparator = _showSeparator;

- (id)initWithDictionary:(NSDictionary *)dict
{
    if ((self = [super initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil]))
    {
        _dict = dict;
        
        self.selectionStyle = UITableViewCellSelectionStyleNone;
        
        self.backgroundColor = [UIColor colorWithWhite:0.1 alpha:0.005];
        
        self.textLabel.textColor = [UIColor blackColor];
        self.textLabel.textAlignment = NSTextAlignmentLeft;
        self.textLabel.adjustsFontSizeToFitWidth = YES;
        self.textLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
        
        NSString *title = [dict objectForKey:SWC_ACCOUNT_KEY];
        self.textLabel.text = title ? title : NSLocalizedString(@"--", nil);
        
        self.detailTextLabel.textColor = [UIColor darkGrayColor];
        self.detailTextLabel.textAlignment = NSTextAlignmentLeft;
        self.detailTextLabel.adjustsFontSizeToFitWidth = YES;
        self.detailTextLabel.baselineAdjustment = UIBaselineAdjustmentAlignCenters;
        
        NSString *subtitle = [dict objectForKey:SWC_SERVER_KEY];
        self.detailTextLabel.text = subtitle ? subtitle : NSLocalizedString(@"--", nil);
        
        self.backgroundView = [[UIView alloc] init];
        self.backgroundView.backgroundColor = self.backgroundColor;
        
        self.imageView.image = [self _checkmarkImage: NO];
        self.imageView.hidden = YES;
        
    }
    
    return self;
}

- (void)setTicked: (BOOL) selected
{
    _isTicked = selected;
}

- (void)layoutSubviews
{
    
    if (_bottomLine) {
        CGFloat scale = [[UIScreen mainScreen] scale];
        [_bottomLine setFrame:CGRectMake(0, self.frame.size.height - (1 / scale), self.frame.size.width, 1 / scale)];
    }
    
    if (_bottomLineSelected) {
        CGFloat scale = [[UIScreen mainScreen] scale];
        [_bottomLineSelected setFrame:CGRectMake(0, self.frame.size.height - (1 / scale), self.frame.size.width, 1 / scale)];
    }
    
    if (_topLine) {
        CGFloat scale = [[UIScreen mainScreen] scale];
        [_topLine setFrame:CGRectMake(0, 0, self.frame.size.width, 1 / scale)];
    }
    
    if (_topLineSelected) {
        CGFloat scale = [[UIScreen mainScreen] scale];
        [_topLineSelected setFrame:CGRectMake(0, 0, self.frame.size.width, 1 / scale)];
    }
    
    if (_isTicked)
    {
        self.imageView.hidden = NO;
    } else {
        self.imageView.hidden = YES;
    }
    
    [super layoutSubviews];
    
}

- (void)setShowSeparator:(BOOL)showSeparator {
    if (_showSeparator != showSeparator) {
        _showSeparator = showSeparator;
        
        if (_showSeparator) {
            if (!_bottomLine) {
                CGRect rectZero = CGRectMake(0, 0, 0, 0);
                _bottomLine = [[UIView alloc] initWithFrame:rectZero];
                _bottomLine.backgroundColor = [UIColor colorWithWhite:.5 alpha:.5];
                [self.backgroundView addSubview:_bottomLine];
            }
            if (!_bottomLineSelected) {
                CGRect rectZero = CGRectMake(0, 0, 0, 0);
                _bottomLineSelected = [[UIView alloc] initWithFrame:rectZero];
                _bottomLineSelected.backgroundColor = [UIColor colorWithWhite:.5 alpha:.5];
                [self.selectedBackgroundView addSubview: _bottomLineSelected];
            }
            
        } else {
            if (_bottomLine) {
                [_bottomLine removeFromSuperview];
                _bottomLine = nil;
            }
            if (_bottomLineSelected) {
                [_bottomLineSelected removeFromSuperview];
                _bottomLineSelected = nil;
            }
        }
    }
}

- (void)setShowTopSeparator:(BOOL)showTopSeparator {
    if (_showTopSeparator != showTopSeparator) {
        _showTopSeparator = showTopSeparator;
        
        if (_showTopSeparator) {
            if (!_topLine) {
                CGRect rectZero = CGRectMake(0, 0, 0, 0);
                _topLine = [[UIView alloc] initWithFrame:rectZero];
                _topLine.backgroundColor = [UIColor colorWithWhite:.5 alpha:.5];
                [self.backgroundView addSubview:_topLine];
            }
            if (!_topLineSelected) {
                CGRect rectZero = CGRectMake(0, 0, 0, 0);
                _topLineSelected = [[UIView alloc] initWithFrame:rectZero];
                _topLineSelected.backgroundColor = [UIColor colorWithWhite:.5 alpha:.5];
                [self.selectedBackgroundView addSubview: _topLineSelected];
            }
            
        } else {
            if (_topLine) {
                [_topLine removeFromSuperview];
                _topLine = nil;
            }
            if (_topLineSelected) {
                [_topLineSelected removeFromSuperview];
                _topLineSelected = nil;
            }
        }
    }
}

@end


//
// SWCViewController
//

@interface SWCViewController ()
{
    NSMutableArray       *_credentials; // array of NSDictionary
    UILabel              *_topLabel;
    UILabel              *_middleLabel;
    UITableView          *_table;
    NSDictionary         *_selectedDict;
    NSIndexPath          *_selectedCell;
}

@end

@implementation SWCViewController

- (NSDictionary *)selectedItem
{
    return _selectedDict;
}

- (void)setCredentials:(NSArray *)inArray
{
    NSMutableArray *credentials = [[NSMutableArray alloc] initWithArray:inArray];
    [credentials sortUsingSelector:@selector(compareCredentialDictionaryAscending:)];
    _credentials = credentials;
    if (_table)
        [_table reloadData];
}

- (void)_enableTable
{
    [_table setUserInteractionEnabled:YES];
}

- (UITableView *)tableView
{
    if (_table == nil) {
        _table = [[UITableView alloc] init];
        [_table setTranslatesAutoresizingMaskIntoConstraints:NO];
        [_table setAutoresizingMask:UIViewAutoresizingNone];
        [_table setBackgroundColor:[UIColor clearColor]];
        [_table setSeparatorStyle:UITableViewCellSeparatorStyleNone];
    }
    [_table sizeToFit];
    
    return (UITableView *)_table;
}

-(void)loadView
{
    
    UIView* view = [[UIView alloc] init];
    
    UITableView* table = [self tableView];
    [table setDelegate: self];
    [table setDataSource: self];
    
    [view addSubview: table];
    
    CFErrorRef error = NULL;
    audit_token_t auditToken = {};
    memset(&auditToken, 0, sizeof(auditToken));
    CFArrayRef credentialList = swca_copy_pairs(swca_copy_pairs_request_id, &auditToken, &error);
    if (error) {
        NSLog(@"Unable to get accounts: %@", [(__bridge NSError*)error localizedDescription]);
    }
    
    [self setCredentials:(__bridge NSArray*)credentialList];
    if (credentialList) {
        CFRelease(credentialList);
    }
    
    
    NSDictionary* views = NSDictionaryOfVariableBindings(table);
    
    if ([_credentials count] > 2)
    {
        
        NSDictionary *metrics = @{@"height":@120.0};
        [view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-[table]-|"  options:0 metrics:metrics views:views]];
        [view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[table(height)]-|" options:0 metrics:metrics views:views]];
        
        CGFloat scale = [[UIScreen mainScreen] scale];
        table.layer.borderWidth = 1.0 / scale;
        table.layer.borderColor = [UIColor colorWithWhite:.5 alpha:.5].CGColor;
        
        [self setPreferredContentSize:CGSizeMake(0,140)];
        
    } else if ([_credentials count] == 2) {
        
        NSDictionary *metrics = @{@"height":@90.0};
        [view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[table]|"  options:0 metrics:metrics views:views]];
        [view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[table(height)]-|" options:0 metrics:metrics views:views]];
        
        [self setPreferredContentSize:CGSizeMake(0,90)];
        [table setScrollEnabled: NO];
        
    } else {  // [_credentials count] == 1
        
        NSDictionary *metrics = @{@"height":@45.0};
        [view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[table]|"  options:0 metrics:metrics views:views]];
        [view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[table(height)]|" options:0 metrics:metrics views:views]];
        
        [self setPreferredContentSize:CGSizeMake(0,45)];
        [table setScrollEnabled: NO];
        
    }
    
    [self setView:view];
}

-(void)viewWillAppear:(BOOL)animated
{
    
    // Select the first cell by default
    
    NSDictionary *dict = [_credentials objectAtIndex: 0];
    _selectedDict = dict;
    _selectedCell = [NSIndexPath indexPathForItem:0 inSection: 0];
    SWCItemCell *cell = (SWCItemCell *)[_table cellForRowAtIndexPath: _selectedCell];
    [cell setTicked: YES];
    [_table selectRowAtIndexPath: _selectedCell animated: NO scrollPosition: UITableViewScrollPositionTop];
    
    CFErrorRef error = NULL;
    audit_token_t auditToken = {};
    memset(&auditToken, 0, sizeof(auditToken));
    bool result = swca_set_selection(swca_set_selection_request_id,
                                     &auditToken, (__bridge CFDictionaryRef)dict, &error);
    if (!result) {
        NSLog(@"Unable to select item: %@", [(__bridge NSError*)error localizedDescription]);
    }
    
    [super viewWillAppear:animated];
}


//
// UITableView delegate methods
//

- (NSInteger)tableView:(UITableView *)table numberOfRowsInSection:(NSInteger)section
{
    return [_credentials count];
}

- (void) tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    NSUInteger row = indexPath.row;
    
    NSDictionary *dict = [_credentials objectAtIndex:row];
    _selectedDict = dict;
    
    CFErrorRef error = NULL;
    audit_token_t auditToken = {};
    memset(&auditToken, 0, sizeof(auditToken));
    bool result = swca_set_selection(swca_set_selection_request_id,
                                     &auditToken, (__bridge CFDictionaryRef)dict, &error);
    if (!result) {
        NSLog(@"Unable to select item: %@", [(__bridge NSError*)error localizedDescription]);
    }
    
    _selectedCell = indexPath;
    SWCItemCell *cell = (SWCItemCell *)[tableView cellForRowAtIndexPath: indexPath];
    [cell setTicked: YES];
    [cell layoutSubviews];
}

- (void) tableView:(UITableView *)tableView didDeselectRowAtIndexPath:(NSIndexPath *)indexPath
{
    SWCItemCell *cell = (SWCItemCell *)[tableView cellForRowAtIndexPath: indexPath];
    [cell setTicked: NO];
    [cell layoutSubviews];
    
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    
    NSDictionary *dict = [_credentials objectAtIndex:[indexPath row]];
    SWCItemCell *cell = [[SWCItemCell alloc] initWithDictionary:dict];
    
    // show separator on top cell if there's only or or two items
    if ([_credentials count] <= 2) {
        cell.showTopSeparator = YES;
    } else {
        cell.showSeparator = YES;
        
        if (indexPath.row == 0)
        {
            cell.showTopSeparator = YES;
        }
        
    }
    
    if (_selectedCell == indexPath)
    {
        [cell setTicked: YES];
    } else {
        [cell setTicked: NO];
    }
    
    return cell;
}


@end
