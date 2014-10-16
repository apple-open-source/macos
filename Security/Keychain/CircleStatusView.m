//
//  CircleStatusView.m
//  Security
//
//  Created by John Hurley on 12/5/12.
//
//

#import "CircleStatusView.h"

@implementation CircleStatusView

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _color = [UIColor redColor];
//      NSLog(@"Frame: w: %f, h: %f", self.frame.size.width, self.frame.size.height);
    }
    return self;
}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
    // Drawing code
}
*/

- (void)drawRect:(CGRect)rect
{
    CGContextRef context= UIGraphicsGetCurrentContext();

//  NSLog(@"Frame: w: %f, h: %f", self.frame.size.width, self.frame.size.height);
    if (!_color)
        _color= [UIColor redColor];
    CGContextSetFillColorWithColor(context, _color.CGColor);
    CGContextSetAlpha(context, 0.95);
    CGContextFillEllipseInRect(context, CGRectMake(0,0,self.frame.size.width,self.frame.size.height));

    CGContextSetStrokeColorWithColor(context, _color.CGColor);
    CGContextStrokeEllipseInRect(context, CGRectMake(0,0,self.frame.size.width,self.frame.size.height));    
}

@end

@implementation ItemStatusView

- (id)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        _color = [UIColor blueColor];
    }
    return self;
}

- (void)drawRect:(CGRect)rect
{
    CGContextRef context= UIGraphicsGetCurrentContext();
    
    //  NSLog(@"Frame: w: %f, h: %f", self.frame.size.width, self.frame.size.height);
    if (!_color)
        _color= [UIColor blueColor];
    CGContextSetFillColorWithColor(context, _color.CGColor);
    CGContextSetAlpha(context, 0.95);
    CGContextFillEllipseInRect(context, CGRectMake(0,0,self.frame.size.width,self.frame.size.height));
    
    CGContextSetStrokeColorWithColor(context, _color.CGColor);
    CGContextStrokeEllipseInRect(context, CGRectMake(0,0,self.frame.size.width,self.frame.size.height));
}

@end
