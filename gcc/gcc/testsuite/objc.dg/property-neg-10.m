/* APPLE LOCAL file radar 4725660 */
/* Check that a bad use of property assignment does not cause an internal error. */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */

@class QZFile;

@interface QZImage
{
}
@property(dynamic) QZFile *imageFile;

- (void) addFile :(QZImage *)qzimage;
@end

@implementation QZImage
- (void) addFile : (QZImage *)qzimage
  {
	qzimage.imageFile.data = 0; /* { dg-error "accessing unknown \\'data\\' component of a property" } */
  }
@end
