//
//  main.m
//  RubyConsole
//
//  Created by Laurent Sansonetti on 7/16/07.
//  Copyright (c) 2007 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <RubyCocoa/RBRuntime.h>

int main(int argc, const char *argv[])
{
    return RBApplicationMain("console.rb", argc, argv);
}
