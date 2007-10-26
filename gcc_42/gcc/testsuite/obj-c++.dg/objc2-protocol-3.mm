/* APPLE LOCAL file 5433974 */
/* { dg-options "-mmacosx-version-min=10.5 -fobjc-abi-version=2" } */
/* { dg-do compile { target *-*-darwin* } } */

@protocol Proto
+classMethod;
@end

@implementation <Proto> @end  /* { dg-error "use of @implementation" } */
/* { dg-warning "@end' must appear in an @implementation context" "" { target *-*-darwin* } 9 } */
