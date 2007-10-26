/** -*-objc-*-
 *
 *   $Id: main.m 979 2006-05-29 01:18:25Z hisa $
 *
 *   Copyright (c) 2001 FUJIMOTO Hisakuni
 *
 **/

#import <RubyCocoa/RBRuntime.h>

int
main(int argc, const char* argv[])
{
  return RBApplicationMain("rb_main.rb", argc, argv);
}
