// $Id: objc_test.m 2183 2008-02-07 16:24:08Z kimuraw $
//
//   some tests require objc codes
#import <Foundation/Foundation.h>

@interface RBThreadTest : NSObject
{
}
@end

@interface NSObject (CallIP)
- (void)call;
@end

@implementation RBThreadTest
-(void)callWithAutoreleasePool:(id)closure {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [closure call];
    [pool release];
}
-(NSException *)callWithExceptionTryCatch:(id)closure {
    NSException *result = nil;
    @try {
        [closure call];
    }
    @catch (NSException * e) {
        result = e;
    }
    return result;
}
@end

@interface RBExceptionTestBase : NSObject
{
}
@end

@implementation RBExceptionTestBase
-(NSException *)testExceptionRoundTrip
{   
  NSException *result = nil;
  NS_DURING
      // defined in test/tc_exception.rb
      [self performSelector:@selector(testExceptionRaise)]; 
  NS_HANDLER
      result = localException;
  NS_ENDHANDLER
  return result;
}
@end

@interface Override : NSObject
{
}
@end

@implementation Override
+(int) foo
{
  return 321;
}
-(int) foo
{   
  return 321;
}
- (NSRect) giveRect
{
  return NSZeroRect;
}
+ (void)testFooOn:(id)instance
{
  int i = [instance foo];
  if (i != 123) 
    [NSException raise:@"TestOverride" format:@"assertion testFooOn failed, expected 123, got %d", i];
}
+ (void)testFooOnClassName:(NSString *)name
{
  [self testFooOn:NSClassFromString(name)];
}
@end

@interface RetainCount : NSObject
{
}
@end

@implementation RetainCount

+(int) rbAllocCount
{
  int retainCount;
  id obj;
  obj = [NSClassFromString(@"RBSubclass") alloc];
  retainCount = [obj retainCount];
  [obj release];
  return retainCount;
}

+(id) ocObjectFromPlaceholder
{
  return [[NSMutableString alloc] init];
}

+(int) rbInitCount
{
  int retainCount;
  id obj;
  obj = [[NSClassFromString(@"RBSubclass") alloc] init];
  retainCount = [obj retainCount];
  [obj release];
  return retainCount;
}

+(id) ocObject
{
  return [[NSObject alloc] init];
}

+(id) rbObject
{
  return [[NSClassFromString(@"RBSubclass") alloc] init];
}

+(int) rbNewCount
{
  int retainCount;
  id obj;
  obj = [NSClassFromString(@"RBSubclass") new];
  retainCount = [obj retainCount];
  [obj release];
  return retainCount;
}

+(id) rbNewObject
{
  return [NSClassFromString(@"RBSubclass") new];
}

@end

// tc_uniqobj.rb
@interface UniqObjC : NSObject
@end

@implementation UniqObjC

- (void)start:(id)o
{
    [o retain];
}

- (id)pass:(id)o
{
    return o;
}

- (void)end:(id)o
{
    [o release];
}

@end

// tc_passbyref.rb
@interface PassByRef : NSObject
@end

@implementation PassByRef

- (BOOL)passByRefObject:(id *)obj
{
    if (obj != NULL) {
        *obj = self;
        return YES;
    }
    return NO;
}

- (BOOL)passByRefObjectWithTypeQualifiers:(inout id *)obj
{
    if (obj != NULL) {
        *obj = self;
        return YES;
    }
    return NO;
}

- (BOOL)passByRefInteger:(int *)integer
{
    if (integer != NULL) {
        *integer = 666;
        return YES;
    }
    return NO;
}

- (BOOL)passByRefFloat:(float *)floating
{
    if (floating != NULL) {
        *floating = 666.0;
        return YES;
    }
    return NO;
}

- (void)passByRefVarious:(id *)object integer:(int *)integer floating:(float *)floating
{
    if (object != NULL)
        *object = self;
    if (integer != NULL)
        *integer = 666;
    if (floating != NULL)
        *floating = 666.0;
}

- (void)passByRefVariousTypeQualifiers:(in id *)object integer:(oneway int *)integer floating:(out float *)floating
{
    if (object != NULL)
        *object = self;
    if (integer != NULL)
        *integer = 333;
    if (floating != NULL)
        *floating = 333.0;
}
@end

// tc_subclass.rb 
@interface __SkipInternalClass : NSData
@end
@implementation __SkipInternalClass {
}
@end

@interface SkipInternalClass : __SkipInternalClass
@end
@implementation SkipInternalClass {
}
@end

@interface CallerClass : NSObject
@end

@interface NSObject (TcSubclassFoo)

- (long)calledFoo:(id)arg;

@end

@implementation CallerClass

- (id)callFoo:(id)receiver
{
    return (id)[receiver calledFoo:nil];
}

@end

@protocol Helper
- (char) testChar:(char) i;
- (int) testInt:(int) i;
- (short) testShort:(short) i;
- (long) testLong:(long) i;
- (float) testFloat:(float) f;
- (double) testDouble:(double) d;
- (long long)testLongLong:(long long)ll;
- (id)foo1;
- (int)foo2:(int)i;
- (void)foo3:(id)ary obj:(id)obj;
- (NSRect)foo4:(NSPoint)point size:(NSSize)size;
- (void)foo5:(NSRect *)rectPtr;
+ (int)superFoo;
@end

@interface TestRig : NSObject { }
- (void) run;
@end

@interface TestObjcExport : NSObject
@end

@implementation TestObjcExport

+ (void)runTests
{
    Class helperClass = NSClassFromString(@"ObjcExportHelper");
    id helper = [[helperClass alloc] init];
    NSRect r;
    int foo;

    foo = [helperClass superFoo];
    if (foo != 42)
      [NSException raise:@"TestObjCExportError" format:@"assertion superFoo failed, expected 42, got %i", foo];

    id s = [helper foo1];
    if (![s isKindOfClass:[NSString class]] || ![s isEqualToString:@"s"])
      [NSException raise:@"TestObjCExportError" format:@"assertion foo1 failed, expected %@, got %@", @"s", s];

    int i = [helper foo2:40];
    if (i != 42)
      [NSException raise:@"TestObjCExportError" format:@"assertion foo2 failed, expected %d, got %d", 42, i];

    id ary = [NSMutableArray array];
    id o = [[NSObject alloc] init];
    [helper foo3:ary obj:o];
    if ([ary count] != 1 || [ary objectAtIndex:0] != o)
      [NSException raise:@"TestObjCExportError" format:@"assertion foo3 failed, object %@ is not in array %@", o, ary];

    foo = 42;
    r = [helper foo4:NSMakePoint(1, 2) size:NSMakeSize(3, 4)];
    if (r.origin.x != 1 || r.origin.y != 2 || r.size.width != 3 || r.size.height != 4)
      [NSException raise:@"TestObjCExportError" format:@"assertion foo4 failed, expected %@, got %@", NSStringFromRect(NSMakeRect(1, 2, 3, 4)), NSStringFromRect(r)];
    if (foo != 42)
      [NSException raise:@"TestObjCExportError" format:@"memory overflow"];

    r = NSMakeRect(1,2,3,4);
    [helper foo5:&r];
    if (r.origin.x != 10 || r.origin.y != 20 || r.size.width != 30 || r.size.height != 40) 
      [NSException raise:@"TestObjCExportError" format:@"assertion foo5 failed, expected %@, got %@", NSStringFromRect(NSMakeRect(10, 20, 30, 40)), NSStringFromRect(r)];
}

@end

@implementation TestRig

- (void) run 
{
    Class helperClass = NSClassFromString(@"RigHelper");
    id helper = [[helperClass alloc] init];

    id name = [helper name];
    if (![name isKindOfClass:[NSString class]] || ![name isEqualToString:@"helper"])
      [NSException raise:@"TestRigError" format:@"assertion name failed, expected %@, got %@", @"helper", name];

    char c = [helper testChar:2];
    if (c != 2)
      [NSException raise:@"TestRigError" format:@"assertion testChar: failed, expected %d, got %d", 2, c];

    int i = [helper testInt:2];
    if (i != 2)
      [NSException raise:@"TestRigError" format:@"assertion testInt: failed, expected %d, got %d", 2, i];

    short s = [helper testShort:2];
    if (s != 2)
      [NSException raise:@"TestRigError" format:@"assertion testShort: failed, expected %hd, got %hd", 2, s];

    long l = [helper testLong:2];
    if (l != 2)
      [NSException raise:@"TestRigError" format:@"assertion testLong: failed, expected %ld, got %ld", 2, l];

    float f2 = 3.141592;
    float f = [helper testFloat:f2];
    if (f != f2)
      [NSException raise:@"TestRigError" format:@"assertion testFloat: failed, expected %f, got %f", f2, f];

    double d2 = 6666666.666;
    double d = [helper testDouble:d2];
    if (d != d2)
      [NSException raise:@"TestRigError" format:@"assertion testDouble: failed, expected %lf, got %lf", d2, d];

    long long ll2 = 1000000000;
    long long ll = [helper testLongLong:ll2];
    if (ll != ll2)
      [NSException raise:@"TestRigError" format:@"assertion testLongLong: failed, expected %lld, got %lld", ll2, ll];
}

@end

@interface TestRBObject : NSObject
@end

@implementation TestRBObject

- (long)addressOfObject:(id)obj
{
  return (long)obj;
}

@end

@interface DirectOverride : NSObject
@end

@implementation DirectOverride

+ (id)classOverrideMe
{
  return nil;
}

- (id)overrideMe
{
  return nil;
}

+ (void)checkOverridenMethods
{
  id obj;

  obj = [DirectOverride classOverrideMe];

  if (![obj isEqualToString:@"bar"])
    [NSException raise:@"DirectOverride" format:@"assertion classOverrideMe failed, got %@", obj];

  id directOverride = [[DirectOverride alloc] init];
  obj = [directOverride overrideMe];
  [directOverride release];

  if (![obj isEqualToString:@"foo"])
    [NSException raise:@"DirectOverride" format:@"assertion overrideMe failed, got %@", obj];
}

@end


// This needs to be separate from DirectOverride since the test might damage
// this class.
@interface DirectOverrideParent : NSObject
@end

@implementation DirectOverrideParent

- (id)overrideMe
{
  return @"foo";
}

- (void)checkOverride:(NSString *)want
{
  id obj = [self overrideMe];

  if (![obj isEqualToString:want])
    [NSException raise:@"DirectOverrideInheritance"
      format:@"assertion overrideMe failed, got %@", obj];
}

@end

@interface DirectOverrideChild : DirectOverrideParent
@end

@implementation DirectOverrideChild
@end


#import <AddressBook/ABPeoplePickerC.h>

@interface TestFourCharCode : NSObject
@end

@implementation TestFourCharCode

+ (int)kEventClassABPeoplePickerValue
{
  return kEventClassABPeoplePicker;
}

@end

@interface ObjcTestStret : NSObject
@end

@protocol StretHelper
- (NSRect)overrideMe:(int)x;
@end

@implementation ObjcTestStret

+ (void)run
{
  Class helperClass = NSClassFromString(@"TestStret");
  id helper = [[helperClass alloc] init];
  NSRect rect;
  int foo = 42;

  rect = [helper overrideMe:50];
  if (!NSEqualRects(rect, NSMakeRect(50, 50, 50, 50))) 
    [NSException raise:@"TestObjCExportError" format:@"assertion overrideMe failed, got %@", NSStringFromRect(rect)];
  if (foo != 42)
    [NSException raise:@"TestObjCExportError" format:@"memory overflow"];
}

- (NSRect)overrideMe:(int)x
{
  return NSZeroRect;
}

@end

@interface TestMagicCookie : NSObject
@end

@implementation TestMagicCookie
+ (BOOL)isKCFAllocatorUseContext:(id)ocid { 
  return ((unsigned long)ocid == (unsigned long)kCFAllocatorUseContext);
}
@end

@interface TestThreadedCallback : NSObject
@end

@interface NSObject (ThreadedInterface)
- (id)threaded;
- (void)done;
@end

static BOOL TestThreadedCallbackDone = NO;

@implementation TestThreadedCallback

+ (void)threadedCallback:(id)obj
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSNumber *number = [obj threaded];
  //NSLog (@"got %@", number);
  if (![number isKindOfClass:[NSNumber class]] || [number intValue] != 42)
    [NSException raise:@"TestThreadedCallbackError" format:@"assertion threaded failed, expected a NSNumber with a 42 value, got %@", number];
  [pool release];
  TestThreadedCallbackDone = YES;
}

+ (void)callbackOnThreadRubyObject:(id)obj 
{
  TestThreadedCallbackDone = NO;
  [NSThread detachNewThreadSelector:@selector(threadedCallback:) toTarget:self withObject:obj];
  while (!TestThreadedCallbackDone && [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.2]]) {}
}

@end

@interface HybridClass : NSObject
@end

@interface NSObject (HybridClassIf)

- (id)imethod;
+ (id)cmethod;

@end

@implementation HybridClass

+ (void)startTests
{
  NSNumber *number; 
  HybridClass *hybrid;
 
  number = [HybridClass cmethod];
  if (![number isEqual:[NSNumber numberWithInt:42]])
    [NSException raise:@"TestHybridClass" format:@"assertion cmethod failed, expected 42, got %@", number];

  hybrid = [[self alloc] init];
  number = [hybrid imethod];
  [hybrid release];
  if (![number isEqual:[NSNumber numberWithInt:42]])
    [NSException raise:@"TestHybridClass" format:@"assertion imethod failed, expected 42, got %@", number];
}

@end


@interface ObjcDataClass : NSObject
@end

@implementation ObjcDataClass
- (BOOL)boolNo { return NO; }
- (BOOL)boolYes { return YES; }
- (bool)cppBoolFalse { return false; }
- (bool)cppBoolTrue { return true; }
- (int)intZero { return 0; }
- (int)intOne { return 1; }
- (int)int42 { return 42; }
- (int)intMinusOne { return -1; }
- (unsigned int)biguint { return 2147483650UL; }
- (char)charZero { return (char)0; }
- (char)charOne { return (char)1; }
- (float)floatTwo { return 2.0f; }
- (double)doubleTwo { return 2.0; }
@end

@interface NSObject (TypeConversionTest)
- (BOOL)testBool;
- (char)testChar;
- (int)testInt;
- (float)testFloat;
- (double)testDouble;
@end


@interface ObjcConversionTest : NSObject
@end

@implementation ObjcConversionTest

- (NSString*)callbackTestBool:(id)target
{
  BOOL b = [target testBool];
  return [NSString stringWithFormat:@"%d", b];
}

- (NSString*)callbackTestChar:(id)target
{
  char c = [target testChar];
  return [NSString stringWithFormat:@"%d", c];
}

- (NSString*)callbackTestInt:(id)target
{
  int i = [target testInt];
  return [NSString stringWithFormat:@"%d", i];
}

- (NSString*)callbackTestFloat:(id)target
{
  float f = [target testFloat];
  return [NSString stringWithFormat:@"%1.1f", f];
}

- (NSString*)callbackTestDouble:(id)target
{
  double d = [target testDouble];
  return [NSString stringWithFormat:@"%1.1lf", d];
}

@end

@interface NSObject (ObjcToRubyCacheTestUtility)
+ (BOOL)findInOcidToRbobjCache:(id)ocid;
- (void)callback:(id)obj;
@end

@interface ObjcToRubyCacheTest : NSObject
@end

@implementation ObjcToRubyCacheTest

+ (BOOL)testObjcToRubyCacheFor:(Class)klass with:(id)target
{
  id obj = [[klass alloc] init];
  [target callback:obj];
  [obj release];
  Class probe = [[NSBundle mainBundle] classNamed:@"RBCacheTestProbe"];
  return [probe findInOcidToRbobjCache:obj];
}

@end

@interface OvmixArgRetained : NSObject
@end

@implementation OvmixArgRetained

+ (void)test:(id)obj
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSObject *o = [[[NSObject alloc] init] autorelease]; 
  [obj performSelector:@selector(setObject:) withObject:o];
  if ([obj performSelector:@selector(getObject)] != o)
    [NSException raise:@"OvmixArgRetained" format:@"assertion1 failed"];
  [pool release];
  if ([obj performSelector:@selector(getObject)] != o)
    [NSException raise:@"OvmixArgRetained" format:@"assertion2 failed"];
}

@end

@interface ObjcPtrTest : NSObject
@end

@implementation ObjcPtrTest

- (void*)returnVoidPtrForArrayOfString
{
  NSString* str = [NSString stringWithUTF8String:"foobar"];
  NSArray* ary = [NSArray arrayWithObject:str];
  [ary retain];
  return (void*)ary;
}

- (void*)returnVoidPtrForKCFBooleanTrue
{
  return (void*)kCFBooleanTrue;
}

- (void*)returnVoidPtrForKCFBooleanFalse
{
  return (void*)kCFBooleanFalse;
}

- (void*)returnVoidPtrForInt
{
  static int i = -2147483648U;
  return (void*)&i;
}

- (void*)returnVoidPtrForUInt
{
  static unsigned int i = 4294967295U;
  return (void*)&i;
}

- (void*)returnVoidPtrForCStr
{
  static const char* str = "foobar";
  return (void*)str;
}

@end

struct ttype1 {float a; float b;};
struct ttype2 {float a[2];};

@implementation NSObject (FooTests)
- (struct ttype1)test1 {
	struct ttype1 r = {1., 2.};
	return r;
}
- (struct ttype2)test2 {
	struct ttype2 r;
	r.a[0] = 1.;
	r.a[1] = 2.;
	return r;
}
@end

void Init_objc_test(){
  // dummy initializer for ruby's `require'
}
