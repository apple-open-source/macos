/* APPLE LOCAL file */
#include <stdio.h>
#include <string.h>

/* Tests interaction with pragma pack, and interleaving of bitfields 
   and non-bitfields.  */

/* { dg-do run { target powerpc*-*-darwin* } } */

#pragma reverse_bitfields on
typedef struct
{
	union
		{
		unsigned int i1;
		struct
			{
			int b1: 1;
			unsigned int b2: 2;
			unsigned int b3: 4;
			unsigned int b4: 8;
			int b5: 16;
			} bits;
		} u1;

	short baz;
	union
		{
		struct
			{
			unsigned int i2;
			unsigned int i3;
			unsigned int i4;
			unsigned int i5;
			} ints;

#pragma pack(1)
		struct
			{
			unsigned int b1: 16;
			unsigned int b2: 8;
			unsigned int b3: 4;
			char quux;
			unsigned int b4: 2;
			unsigned int b5: 1;
			short baz;
			unsigned int b6: 2;
			unsigned int b7: 4;
			short baz2;
			unsigned int b8: 8;
			unsigned int b9: 16;
			} bits;
		} u2;
} Bitfields;


int main()
{
	Bitfields bitfields;


	memset(&bitfields, 0, sizeof(bitfields));

	bitfields.u1.bits.b1 = 1; /* { dg-warning "overflow in implicit constant conversion" } */
	bitfields.u1.bits.b2 = 1;
	bitfields.u1.bits.b3 = 1;
	bitfields.u1.bits.b4 = 1;
	bitfields.u1.bits.b5 = 1;
	bitfields.baz = 0x5555;
	bitfields.u2.bits.b1 = 1;
	bitfields.u2.bits.b2 = 1;
	bitfields.u2.bits.b3 = 1;
        bitfields.u2.bits.quux = 0xa55a; /* { dg-warning "overflow in implicit constant conversion" } */
	bitfields.u2.bits.b4 = 1;
	bitfields.u2.bits.b5 = 1;
	bitfields.u2.bits.baz = 0xaaaa; /* { dg-warning "overflow in implicit constant conversion" } */
	bitfields.u2.bits.b6 = 1;
	bitfields.u2.bits.b7 = 1;
	bitfields.u2.bits.baz2 = 0x3333;
	bitfields.u2.bits.b8 = 1;
	bitfields.u2.bits.b9 = 1;

	if (bitfields.u1.i1 != 0x0000808b
	    || bitfields.baz != 0x5555
	    || bitfields.u2.ints.i2 != 0x01010001
	    || bitfields.u2.ints.i3 != 0x5a00aaaa
	    || bitfields.u2.ints.i4 != 0x01333305
	    || bitfields.u2.ints.i5 != 0x00000001
	    || sizeof(bitfields) != 22)
	  return 42;
    return 0;
}
