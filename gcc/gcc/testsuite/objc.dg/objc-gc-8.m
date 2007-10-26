/* APPLE LOCAL file radar 4600999 */
/* Test that addition of __strong attribute to actual argument does not
   result in a warning in objc or ICE in objc++ mode. */
/* { dg-do compile } */
/* { dg-options "-fnext-runtime -fobjc-gc" } */

typedef  const struct _CFURLRequest*     CONSTRequestRef;

int Policy (CONSTRequestRef);

@interface NSURL 
{
    @public
    __strong  const struct _CFURLRequest*  request;
}
@end

int FOO(NSURL *_internal)
{
    return Policy(_internal->request);
}
