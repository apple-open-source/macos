/* APPLE LOCAL file Macintosh alignment */

/* { dg-do run } */
/* { dg-options "-Wno-long-long" } */

/*
 * Macintosh compiler alignment test for C.
 * Fred Forsman
 * Apple Computer, Inc.
 * (C) 2000-2002.
 * Last modified 2002-4-29
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#define Q(x) #x, x

typedef unsigned char UINT8;

static int bad_option = 0;
static int flag_verbose = 0;
static int nbr_failures = 0;

/* === basic types === */

typedef struct B1 {
    char		f1;
    UINT8		f2;
} B1;

typedef struct B2 {
    short		f1;
    UINT8		f2;
} B2;

typedef struct B3 {
    long		f1;
    UINT8		f2;
} B3;

typedef struct B4 {
    int			f1;
    UINT8		f2;
} B4;

typedef struct B5 {
    float		f1;
    UINT8		f2;
} B5;

/* doubles, long longs, and vectors are treated separately below. */

/* === enums === */

typedef enum E1 {
  e1_b = 0,
  e1_e = 255
} E1;

typedef enum E2 {
  e2_b = -256,
  e2_e = 255
} E2;

typedef enum E3 {
  e3_b = 0,
  e3_e = 32767
} E3;

typedef enum E4 {
  e4_b = 0,
  e4_e = 65536
} E4;

/* === pointers === */

typedef struct P1 {
    char *		f1;
    UINT8		f2;
} P1;

typedef struct P2 {
    long *		f1;
    UINT8		f2;
} P2;

typedef struct P3 {
    double *	f1;
    UINT8		f2;
} P3;

typedef struct P4 {
    long long *	f1;
    UINT8		f2;
} P4;

typedef struct P5 {
    void (*	f1) (void);
    UINT8		f2;
} P5;

#ifdef __VEC__
typedef struct P61 {
    vector signed short *	f1;
    UINT8		f2;
} P6;
#endif

/* === vectors === */

#ifdef __VEC__
typedef struct V1 {
    vector signed short f1;
    UINT8		f2;
} V1;

typedef struct V2 {
    V1		f1;
    UINT8	f2;
} V2;

typedef struct V3 {
    UINT8		f1;
    vector signed short f2;
} V3;

typedef struct V4 {
    V3		f1;
    UINT8	f2;
} V4;
#endif

/* === doubles === */

typedef struct D1 {
    double	f1;
    UINT8	f2;
} D1;

typedef struct D2 {
    D1		f1;
    UINT8	f2;
} D2;

typedef struct D3 {
    UINT8	f1;
    double	f2;
} D3;

typedef struct D4 {
    D3		f1;
    UINT8	f2;
} D4;

typedef struct D5 {
    UINT8	f1;
    D3		f2;
} D5;

typedef struct D6 {
    double	f1;
    UINT8	f2;
    double	f3;
} D6;

typedef struct D7 {
    UINT8	f1;
    D1		f2;
} D7;

/* === long longs === */

typedef struct LL1 {
    long long	f1;
    UINT8	f2;
} LL1;

typedef struct LL2 {
    LL1		f1;
    UINT8	f2;
} LL2;

typedef struct LL3 {
    UINT8	f1;
    long long	f2;
} LL3;

typedef struct LL4 {
    LL3		f1;
    UINT8	f2;
} LL4;

typedef struct LL5 {
    UINT8	f1;
    LL3		f2;
} LL5;

/* === arrays === */

typedef struct A1 {
    short	f1[4];
    UINT8	f2;
} A1;

typedef struct A2 {
    A1		f1;
    UINT8	f2;
} A2;

typedef struct A3 {
    double	f1[4];
    UINT8	f2;
} A3;

typedef struct A4 {
    A3		f1;
    UINT8	f2;
} A4;

typedef struct A5 {
    long long	f1[4];
    UINT8	f2;
} A5;

typedef struct A6 {
    A5		f1;
    UINT8	f2;
} A6;

#ifdef __VEC__
typedef struct A7 {
    vector signed short	f1[4];
    UINT8		f2;
} A7;

typedef struct A8 {
    A7		f1;
    UINT8	f2;
} A8;
#endif

typedef struct A9 {
    D1		f1[4];
    UINT8	f2;
} A9;

typedef struct A10 {
    A9		f1;
    UINT8	f2;
} A10;

/* === unions === */

typedef union U1 {
    UINT8	f1;
    double	f2;
} U1;

typedef struct U2 {
    U1		f1;
    UINT8	f2;
} U2;

typedef union U3 {
    UINT8	f1;
    long long	f2;
} U3;

typedef struct U4 {
    U3		f1;
    UINT8	f2;
} U4;

#ifdef __VEC__
typedef union U5 {
    UINT8		f1;
    vector signed short	f2;
} U5;

typedef struct U6 {
    U5		f1;
    UINT8	f2;
} U6;
#endif

typedef union U7 {
    UINT8	f1;
    short	f2[4];
} U7;

typedef struct U8 {
    U7		f1;
    UINT8	f2;
} U8;

/* === misc === */

typedef struct {			/* unnamed struct */
    long long	f1;
    UINT8	f2;
} M0;

typedef struct M1 {
    UINT8	f1[8];
} M1;

typedef struct M2 {
    M1		f1;
    UINT8	f2;
} M2;

typedef struct M3 {
    UINT8	f1;
    M1		f2;
} M3;

typedef struct M4 {			/* M4 & M5: see corresponding mac68k tests (M68K11 & M68K12) */
    UINT8	f1[9];
} M4;

typedef struct M5 {
    UINT8	f1;
    M4		f2;
} M5;

/* === mac68k === */

#pragma options align=mac68k

typedef struct M68K0 {
    long	f1;
    UINT8	f2;
} M68K0;

typedef struct M68K1 {
    double	f1;
    UINT8	f2;
} M68K1;

#pragma options align=reset

typedef struct M68K2 {
    M68K1	f1;
    UINT8	f2;
} M68K2;

#ifdef __VEC__
#pragma options align=mac68k

typedef struct M68K3 {
    vector signed short	f1;
    UINT8		f2;
} M68K3;

typedef struct M68K4 {
    M68K3	f1;
    UINT8	f2;
} M68K4;

#pragma options align=reset

typedef struct M68K5 {
    M68K3	f1;
    UINT8	f2;
} M68K5;

#pragma options align=mac68k

typedef struct M68K6 {
    UINT8		f1;
    vector signed short	f2;
} M68K6;

#pragma options align=reset
#endif

#pragma options align=mac68k

typedef struct M68K7 {
    UINT8	f1;
} M68K7;

typedef union M68K8 {
    UINT8	f1;
} M68K8;

typedef struct M68K9 {
    UINT8	f1;
    int		f2;
    UINT8	f3;
} M68K9;

#pragma options align=reset

typedef struct M68K10 {
    UINT8	f1;
    M68K9	f2;
} M68K10;

#pragma options align=mac68k

typedef struct M68K11 {			/* M68K11 & M68K12: see corresponding power tests (M4 & M5) */
    UINT8	f1[9];
} M68K11;

typedef struct M68K12 {
    UINT8	f1;
    M68K11	f2;
} M68K12;

typedef struct M68K13 {
    UINT8	f1;
    UINT8	f2[5];
} M68K13;

#pragma options align=reset


static void check(char * rec_name, int actual, int expected, char * comment)
{
    if (flag_verbose || (actual != expected)) {
        printf("%-20s = %2d (%2d) ", rec_name, actual, expected);
        if (actual != expected) {
            printf("*** FAIL");
            nbr_failures++;
        } else
            printf("    PASS");
        printf(": %s\n", comment);
    }
}

static void check_option(char *option)
{
    if (*option == '-') {
        if (strcmp(option, "-v") == 0)
            flag_verbose = 1;
        else {
            fprintf(stderr, "*** unrecognized option '%s'.\n", option);
            bad_option = 1;
        }
    } else {
        fprintf(stderr, "*** unrecognized option '%s'.\n", option);
        bad_option = 1;
    }
}

int main(int argc, char *argv[])
{
    int i;
    
    for (i = 1; i < argc; i++)
        check_option(argv[i]);
    
    if (bad_option)
        return 1;
        
    /* === basic data types === */
    
    check(Q(sizeof(char)), 1, "char data type");
    check(Q(sizeof(signed char)), 1, "signed char data type");
    check(Q(sizeof(unsigned char)), 1, "unsigned char data type");
    check(Q(sizeof(short)), 2, "short data type");
    check(Q(sizeof(signed short)), 2, "signed short data type");
    check(Q(sizeof(unsigned short)), 2, "unsigned short data type");
    check(Q(sizeof(long)), 4, "short long type");
    check(Q(sizeof(signed long)), 4, "signed long data type");
    check(Q(sizeof(unsigned long)), 4, "unsigned long data type");
    check(Q(sizeof(int)), 4, "short int type");
    check(Q(sizeof(signed int)), 4, "signed int data type");
    check(Q(sizeof(unsigned int)), 4, "unsigned int data type");
    check(Q(sizeof(float)), 4, "float type");
    check(Q(sizeof(double)), 8, "double data type");
    check(Q(sizeof(long long)), 8, "long long data type");
    check(Q(sizeof(signed long long)), 8, "signed long long data type");
    check(Q(sizeof(unsigned long long)), 8, "unsigned long long data type");

    check(Q(sizeof(B1)), 2, "char as 1st field");
    check(Q(sizeof(B2)), 4, "short as 1st field");
    check(Q(sizeof(B3)), 8, "long as 1st field");
    check(Q(sizeof(B4)), 8, "int as 1st field");
    check(Q(sizeof(B5)), 8, "float as 1st field");

    /* === enums === */
    
    check(Q(sizeof(E1)), 4, "enum with range 0..255");
    check(Q(sizeof(E2)), 4, "enum with range -256..255");
    check(Q(sizeof(E3)), 4, "enum with range 0..32767");
    check(Q(sizeof(E4)), 4, "enum with range 0..65536");

    /* === pointers === */
    
    check(Q(sizeof(P1)), 8, "char * as 1st field");
    check(Q(sizeof(P2)), 8, "long * as 1st field");
    check(Q(sizeof(P3)), 8, "double * as 1st field");
    check(Q(sizeof(P4)), 8, "long long * as 1st field");
    check(Q(sizeof(P5)), 8, "function * as 1st field");
    
#ifdef __VEC__
    check(Q(sizeof(P6)), 8, "vector signed short * as 1st field");
#endif

#ifdef __VEC__
    /* === vectors === */
    
    /* ??? Do we want to test all the possible vector data types? ??? */
    check(Q(sizeof(vector signed short)), 16, "vector signed short data type");

    check(Q(sizeof(V1)), 32, "vector as 1st field");
    check(Q(sizeof(V2)), 48, "embedding struct with vector as 1st field");
    check(Q(sizeof(V3)), 32, "vector as 2nd field");
    check(Q(offsetof(V3, f2)), 16, "offset of vector as 2nd field");
    check(Q(sizeof(V4)), 48, "embedding struct with vector as 2nd field");
#endif
    
    /* === doubles === */
    
    check(Q(sizeof(D1)), 16, "double as 1st field");
    check(Q(sizeof(D2)), 24, "embedding struct with double as 1st field");
    check(Q(sizeof(D3)), 12, "double as 2nd field");
    check(Q(offsetof(D3, f2)), 4, "offset of double as 2nd field");
    check(Q(sizeof(D4)), 16, "embedding struct with double as 2nd field");
    check(Q(sizeof(D5)), 16, "struct with double as 2nd field");
    check(Q(offsetof(D5, f2)), 4, "offset of struct with double as 2nd field");
    check(Q(sizeof(D6)), 24, "struct with double, char, double");
    check(Q(offsetof(D6, f3)), 12, "offset of 2nd double in struct with double, char, double");
    check(Q(sizeof(D7)), 20, "struct with double as 2nd field of another struct");
    check(Q(offsetof(D7, f2)), 4, "offset of struct with double as 2nd field of another struct");
   
    /* === long longs === */
    
    check(Q(sizeof(LL1)), 16, "long long as 1st field");
    check(Q(sizeof(LL2)), 24, "embedding struct with long long as 1st field");
    check(Q(sizeof(LL3)), 12, "long long as 2nd field");
    check(Q(offsetof(LL3, f2)), 4, "offset of long long as 2nd field");
    check(Q(sizeof(LL4)), 16, "embedding struct with long long as 2nd field");
    check(Q(sizeof(LL5)), 16, "struct with long long as 2nd field");
    check(Q(offsetof(LL5, f2)), 4, "offset of struct with long long as 2nd field");

    /* === arrays === */
    
    check(Q(sizeof(A1)), 10, "array of shorts as 1st field");
    check(Q(sizeof(A2)), 12, "embedding struct with array of shorts as 1st field");
    check(Q(sizeof(A3)), 40, "array of doubles as 1st field");
    check(Q(sizeof(A4)), 48, "embedding struct with array of doubles as 1st field");
    check(Q(sizeof(A5)), 40, "array of long longs as 1st field");
    check(Q(sizeof(A6)), 48, "embedding struct with array of long longs as 1st field");
#ifdef __VEC__
    check(Q(sizeof(A7)), 80, "array of vectors as 1st field");
    check(Q(sizeof(A8)), 96, "embedding struct with array of vectors as 1st field");
#endif
    check(Q(sizeof(A9)), 72, "array of structs as 1st field");
    check(Q(sizeof(A10)), 80, "embedding struct with array of structs as 1st field");

    /* === unions === */
    
    check(Q(sizeof(U1)),  8, "union with double");
    check(Q(sizeof(U2)), 16, "embedding union with double");
    check(Q(sizeof(U3)),  8, "union with long long");
    check(Q(sizeof(U4)), 16, "embedding union with long long");
#if __VEC__
    check(Q(sizeof(U5)), 16, "union with vector");
    check(Q(sizeof(U6)), 32, "embedding union with vector");
#endif
    check(Q(sizeof(U7)),  8, "union with array of shorts");
    check(Q(sizeof(U8)), 10, "embedding union with array of shorts");

    /* === misc === */
    
    check(Q(sizeof(M0)), 16, "untagged struct with long long as 1st field");
    check(Q(sizeof(M1)),  8, "array[8] of char");
    check(Q(sizeof(M2)),  9, "embedding struct with array[8] of char as 1st field");
    check(Q(sizeof(M3)),  9, "embedding struct with array[8] of char as 2nd field");
    check(Q(offsetof(M3, f2)), 1, "offset of struct with array[8] of char as 2nd field");
    check(Q(sizeof(M4)),  9, "odd size struct: array[9] of char");
    check(Q(sizeof(M5)), 10, "embedding odd size struct");

    /* === mac68k mode === */
    
    check(Q(sizeof(M68K0)),  6, "mac68k struct with long");
    check(Q(sizeof(M68K1)), 10, "mac68k struct with double as 1st field");
    check(Q(sizeof(M68K2)), 12, "embedding mac68k struct with double as 1st field");
#ifdef __VEC__
    check(Q(sizeof(M68K3)), 32, "mac68k struct with vector as 1st field");
    check(Q(sizeof(M68K4)), 48, "embedding mac68k struct with vector as 1st field in a mac68k struct");
    check(Q(sizeof(M68K5)), 48, "embedding mac68k struct with vector as 1st field in a power struct");
    check(Q(offsetof(M68K6, f2)), 16, "offset of vector as 2nd field in a mac68k struct");
#endif
    check(Q(sizeof(M68K7)), 2, "padding of mac68k struct with one char");
    check(Q(sizeof(M68K8)), 2, "padding of mac68k union with one char");
    check(Q(sizeof(M68K9)), 8, "padding of mac68k struct");
    check(Q(offsetof(M68K9, f2)), 2, "offset of int as 2nd field in a mac68k struct");
    check(Q(sizeof(M68K10)), 10, "power struct with embedded mac68k struct");
    check(Q(offsetof(M68K10, f2)), 2, "offset of mac68k struct as 2nd field in a power struct");
    check(Q(sizeof(M68K11)), 10, "odd size struct (before padding): array[9] of char");
    check(Q(sizeof(M68K12)), 12, "embedding odd size struct (before padding)");
    check(Q(sizeof(M68K13)), 6, "array of char at odd addr in mac68k struct");
    check(Q(offsetof(M68K13, f2)), 1, "offset of array of char at odd addr in mac68k struct");

    if (nbr_failures > 0)
    	return 1;
    else
    	return 0;
}
