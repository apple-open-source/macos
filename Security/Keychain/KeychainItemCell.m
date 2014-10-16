//
//  KeychainItemCell.m
//  Security
//
//  Created by John Hurley on 10/22/12.
//
//

#import "KeychainItemCell.h"
#import <QuartzCore/QuartzCore.h>

@implementation KeychainItemCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if (self) {
        // Initialization code
    }
    return self;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
    [super setSelected:selected animated:animated];

    // Configure the view for the selected state
}

+ (NSString *)reuseIdentifier
{
    return @"KeychainItemCellIdentifier";
}

- (void)startCellFlasher
{
    CABasicAnimation *theAnimation = NULL;
    
    theAnimation=[CABasicAnimation animationWithKeyPath:@"opacity"];
    theAnimation.duration=0.75;
    theAnimation.repeatCount=6;    //HUGE_VALF;
    theAnimation.autoreverses=YES;
    theAnimation.fromValue=[NSNumber numberWithFloat:0.0];
    theAnimation.toValue=[NSNumber numberWithFloat:1.0];
    theAnimation.removedOnCompletion = TRUE;
    [_itemStatus.layer addAnimation:theAnimation forKey:@"animateOpacity"];
}

@end
