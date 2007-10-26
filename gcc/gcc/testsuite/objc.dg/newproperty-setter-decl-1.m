/* APPLE LOCAL file radar 4968128 */
/* Test that user specified setter declaration is generated correctly.
   No error/warning must be issued in this test case. */
/* { dg-options "-fobjc-new-property -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

@interface PSEntry
@property (getter=isRead, setter=setRead:) int read;
@end

int foo (PSEntry *entry)
{
	entry.read = 1 + entry.read;
}
