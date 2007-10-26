/* APPLE LOCAL file radar 5153561 */
/* Check that early use of 'id' in a c++ catch clause with zero-cost exception on 
   will not ICE. */
/* { dg-options "-m64 -mmacosx-version-min=10.5" } */
/* { dg-do compile { target *-*-darwin* } } */

#include <Foundation/Foundation.h>
extern void empty(void);
int main() {
    // @throw (id)0;  // uncomment to prevent crash
    try { empty(); } 
    catch (id e) { empty(); }
}

