/* New ECC curves,

   14 Apr 2001 (REC) ensured x1Minus arithmetic & prime point orders
    5 Apr 2001 (REC) factored minusorder for NIST-P-192
    3 Apr 2001 (REC) first draft

   c. 2001 Apple Computer, Inc.
   All Rights Reserved.

   Currently there are 7 (seven) curves, at varying
   bit-depth and varying parameter types:

   FEE curves (use Montgomery arithmetic and feemod base-prime):  
				31 bits 
				127 bits
   IEEE curves (use projective arithmetic): 
				31 bits  (feemod base-prime)
                128 bits (feemod base-prime) 
				161 bits (feemod base-prime) (default preference) 
				161 bits (general prime) 
				192 bits (general. prime) (NIST-recommended)

   Each curve is given key comments atop the parameters.
   For performance considerations, 

       primeType->Mersenne is faster than primeType->feemod is
           faster than primeType->general

       curveType->Montgomery is faster than curveType->Weierstrass,

   Some choices are not obvious except to cryptographers;
   e.g., the two curves given for 161 bits exist because
   of cryptographic controversies; probably the curve with
   both orders prime is more secure, so it is perhaps
   the curve of choice at 161 bits.

   The parameters/points have standard meaning, except for our
   special entities as listed below.  It is important to note the
   principle thgat, without exception, every CryptKit base prime
   p is = 3 (mod 4).  This allows simple square-rooting in the field
   F_p.  Because of this universal constraint, (-1) is always a
   quadratic nonresidue and so twist curves as below can assume
   g = -1.

     (...)plusOrder :=  The usual elliptic-curve order;
     (...)x1Plus := x-coordinate on y^2 = x^3 + c x^2 + a x + b;
     (...)x1OrderPlus := Order of x1Plus, always divides plusOrder
     (...)minusOrder := Order of the twist curve = 2p+2-plusOrder
     (...)x1Minus := x-coordinate chosen on the twist curve
             g y^2 = x^3 + c x^2 + a x + b
         where g = -1 is the nonresidue, and such that 
         the special, x-coordinates-only, twofold-ambiguous "add" of
         FEED works on the minus curve, using the same curve
         parameters a,b,c as for the plus curve.  Note that
         x1Minus is to be chosen so that the correct "add" arithmetic
         occurs, and also so that the desired point order accrues.
     (...)x1OrderMinus := Order of x1Plus, always divides minusOrder.

    In each of the curves specified below, the plusOrder (at least)
    is prime, while each of the point orders x1OrderPlus/Minus     
    is always prime.
   
   Note that the older labels Atkin3, Atkin4 have been abolished.

 */

 /* FEE CURVE: USE FOR FEE SIG. & FEED ONLY.
  * primeType->Mersenne
  * curveType->Montgomery
  * q = 31;   k = 1;  p = 2^q - k;
  * a = 1;   b = 0;   c = 666;
  * Both orders composite.
  */
static const arrayDigit ga_31m_x1Plus[]   =
	{2, 61780, 6237};
	/* 408809812 */
static const arrayDigit ga_31m_x1Minus[]  =
	{2,12973,30585};
	/* 2004431533 */
static const arrayDigit ga_31m_plusOrder[]  =
	{2, 25928, 32768 };
	/* 2147509576 = 2^3 * 268438697. */
static const arrayDigit ga_31m_minusOrder[]  =
	{2, 39608, 32767 };
	/* 2147457720 = 2^3 * 3 * 5 * 17895481. */
static const arrayDigit ga_31m_x1OrderPlus[] =
	{2, 3241, 4096};
	/* 268438697 */
static const arrayDigit ga_31m_x1OrderMinus[]  =
	{2, 4153, 273};
	/* 17895481 */
static const arrayDigit ga_31m_x1OrderPlusRecip[]  =
	{2, 52572, 16383};
static const arrayDigit ga_31m_lesserX1OrderRecip[]  =
	{2, 759, 960};

 /* IEEE P1363 COMPATIBLE.
  * primeType->Mersenne
  * curveType->Weierstrass
  * q = 31;   k = 1; p = 2^q-k;  
  * a = 5824692    b = 2067311435   c = 0
  * Both orders prime.
  */
static const arrayDigit ga_31w_x1Plus[]   =
	{1, 6 };
static const arrayDigit ga_31w_x1Minus[]  =
	{1, 7 };
static const arrayDigit ga_31w_plusOrder[]  =
	{2,59003,32766 };
	/* 2147411579 */
static const arrayDigit ga_31w_minusOrder[]  =
	{2,6533,32769 };
	/* 2147555717 */
static const arrayDigit ga_31w_x1OrderPlus[] =
	{2,59003,32766};
	/* 2147411579 */
static const arrayDigit ga_31w_x1OrderMinus[]  =
	{2,6533,32769};
	/* 2147555717 */
static const arrayDigit ga_31w_x1OrderPlusRecip[]  =
	{2, 6535, 32769};

static const arrayDigit ga_31w_a[]  =
	{2,57524,88};
	/* 5824692 */
static const arrayDigit ga_31w_b[]  =
	{2,43851,31544};
	/* 2067311435 */

 /* FEE CURVE: USE FOR FEE SIG. & FEED ONLY.
  * primeType->Mersenne
  * curveType->Montgomery
  * q = 127;   k = 1;  p = 2^q - k;
  * a = 1;   b = 0;   c = 666;
  * Both orders composite.
  */
static const arrayDigit ga_127m_x1Plus[]  =
	{8,     24044, 39922, 11050,
	 24692, 34049, 9793,  1228, 31562};
	/* 163879370753099435779911346846180728300 */
static const arrayDigit ga_127m_x1Minus[] =
	{8,49015,6682,26772,63672,45560,46133,24769,8366};
	/* 43440717976631899041527862406676135799 */
static const arrayDigit ga_127m_plusOrder[]  =
	{ 8,     14612, 61088, 34331,
	  32354, 65535, 65535, 65535,
	  32767};
	/* 170141183460469231722347548493196835092 =
2^2 * 3^4 * 71 * 775627 * 9535713005180210505588285449. */
static const arrayDigit ga_127m_minusOrder[] =
	{ 8,     50924, 4447, 31204,
	  33181, 0,     0,    0,
	  32768 };
	/* 170141183460469231741027058938571376364 =
2^2 * 17 * 743 * 1593440383 * 2113371777483973234080067. */
static const arrayDigit ga_127m_x1OrderPlus[] =
	{6,     8201,  61942, 37082,
	 53787, 49605, 7887 };
	/* 9535713005180210505588285449 */
static const arrayDigit ga_127m_x1OrderMinus[] =
	{6,    14659, 1977,16924,
	 7446, 49030, 1};
	/* 2113371777483973234080067 */
static const arrayDigit ga_127m_x1OrderPlusRecip[]  =
	{6, 21911, 8615, 0, 40960, 64107, 8507};
static const arrayDigit ga_127m_lesserX1OrderRecip[]  =
	{6, 44759, 65533, 17695, 61560, 18883, 2};

 /* IEEE P1363 COMPATIBLE.
  * primeType->feemod
  * curveType->Weierstrass
  * q = 127;  k = -57675; p = 2^q - k;
  * a = 170141183460469025572049133804586627403;   
  * b = 170105154311605172483148226534443139403;    c = 0;
  * Both orders prime.:
  */
static const arrayDigit ga_128w_x1Plus[] =
	{1,6};
	/* 6 */
static const arrayDigit ga_128w_x1Minus[] =
	{1,3};
	/* 3 */
static const arrayDigit ga_128w_plusOrder[] =
	{8,40455,13788,48100,24190,1,0,0,32768};
	/* 170141183460469231756943134065055014407. */
static const arrayDigit ga_128w_minusOrder[] =
	{8,9361,51749,17435,41345,65534,65535,65535,32767};
	/* 170141183460469231706431473366713312401. */
static const arrayDigit ga_128w_x1OrderPlus[] =
	{8,40455,13788,48100,24190,1,0,0,32768};
	/* 170141183460469231756943134065055014407. */
static const arrayDigit ga_128w_x1OrderMinus[] =
	{8,9361,51749,17435,41345,65534,65535,65535,32767};
	/* 170141183460469231706431473366713312401. */
static const arrayDigit ga_128w_x1OrderPlusRecip[] =
	{9,34802,10381,4207,34309,65530,65535,65535,65535,1}; 
static const arrayDigit ga_128w_lesserX1OrderRecip[] =
	{8,56178,13786,48100,24190,1,0,0,32768};

static const arrayDigit ga_128w_a[] =  
	{8,29003,44777,29962,4169,54360,65535,65535,32767};
	/* 170141183460469025572049133804586627403; */
static const arrayDigit ga_128w_b[] =   
	{8,16715,42481,16221,60523,56573,13644,4000,32761};
	/* 170105154311605172483148226534443139403. */

 /* IEEE P1363 COMPATIBLE.
  * primeType->feemod
  * curveType->Weierstrass
  * q = 160;  k = -5875; p = 2^q - k;
  * a = 1461501637330902918203684832716283019448563798259;   
  * b = 36382017816364032;    c = 0;
  * Both orders prime.:
  */
static const arrayDigit ga_161w_x1Plus[] =
	{1,7};
	/* 7 */
static const arrayDigit ga_161w_x1Minus[] =
	{1,4};
	/* 4 */
static const arrayDigit ga_161w_plusOrder[] =
	{11,50651,30352,49719,403,64085,1,0,0,0,0,1};
	/* 1461501637330902918203687223801810245920805144027. */
static const arrayDigit ga_161w_minusOrder[] =
	{10,26637,35183,15816,65132,1450,65534,65535,65535,65535,65535};
	/* 1461501637330902918203682441630755793391059953677. */
static const arrayDigit ga_161w_x1OrderPlus[] =
	{11,50651,30352,49719,403,64085,1,0,0,0,0,1};
	/* 1461501637330902918203687223801810245920805144027. */
static const arrayDigit ga_161w_x1OrderMinus[] =
	{10,26637,35183,15816,65132,1450,65534,65535,65535,65535,65535};
	/* 1461501637330902918203682441630755793391059953677. */
static const arrayDigit ga_161w_x1OrderPlusRecip[] =
	{11,59555,9660,63266,63920,5803,65528,65535,65535,65535,65535,3};
/* added by dmitch */
static const arrayDigit ga_161w_lesserX1OrderRecip[] =
	{12,38902,30352,49719,403,64085,1,0,0,0,0,1,0};
/* end addenda */

static const arrayDigit ga_161w_a[] =  {10,4339,47068,65487,65535,65535,65535,65535,65535,65535,65535};
/* 1461501637330902918203684832716283019448563798259; */
static const arrayDigit ga_161w_b[] =    {4,1024,41000,16704,129};
/* 36382017816364032. */

 /* IEEE P1363 COMPATIBLE.
  * primeType->General
  * curveType->Weierstrass
  * p is a 161-bit random prime (below, ga_161_gen_bp[]);
  * a = -152;   b = 722;    c = 0;
  * Both orders composite.:
  */
static const arrayDigit ga_161_gen_bp[] =
	{11,41419,58349,36408,14563,25486,9098,29127,50972,7281,8647,1};
	/* baseprime = 1654338658923174831024422729553880293604080853451 */
static const arrayDigit ga_161_gen_x1Plus[] =
	{10,59390,38748,49144,50217,32781,46057,53816,62856,18968,55868};
	/* 1245904487553815885170631576005220733978383542270 */
static const arrayDigit ga_161_gen_x1Minus[] =
	{10,12140,40021,9852,49578,18446,39468,28773,10952,26720,52624};
   /* 1173563507729187954550227059395955904200719019884 */
static const arrayDigit ga_161_gen_plusOrder[] =
	{11,41420,58349,36408,14563,25486,9100,29127,50972,7281,8647,1};
	/* 1654338658923174831024425147405519522862430265804 =
   2^2 * 23 * 359 * 479 * 102107 * 1024120625531724089187207582052247831. */
static const arrayDigit ga_161_gen_minusOrder[] =
	{11,41420,58349,36408,14563,25486,9096,29127,50972,7281,8647,1};
	/* 1654338658923174831024420311702241064345731441100 =
2^2 * 5^2 * 17^2 * 57243552211874561627142571339177891499852299. */
static const arrayDigit ga_161_gen_x1OrderPlus[] =
	{8,59671,64703,58305,55887,34170,37971,15627,197};
	/* 1024120625531724089187207582052247831 */
static const arrayDigit ga_161_gen_x1OrderMinus[] =
	{10,49675,56911,64364,6281,5543,59511,52057,44604,37151,2};
	/* 57243552211874561627142571339177891499852299 */
static const arrayDigit ga_161_gen_x1OrderPlusRecip[] =
	{8, 7566, 37898, 14581, 2404, 52670, 23839, 17554, 332};

static const arrayDigit ga_161_gen_a[] =    {-1, 152};	/* a = -152 */
static const arrayDigit ga_161_gen_b[] =    { 1, 722};	/* b = 722 */


 /* IEEE P1363 COMPATIBLE.
  * (NIST-P-192 RECOMMENDED PRIME)
  * primeType->General
  * curveType->Weierstrass
  * p is a 192-bit prime (with efficient bit structure) (below, ga_192_gen_bp[]);
  * a = -3;   b = 2455155546008943817740293915197451784769108058161191238065;    c = 0;
  * Plus-order is prime, minus-order is composite.
  */
static const arrayDigit ga_192_gen_bp[] =
	{12,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65535};
	/* baseprime =
6277101735386680763835789423207666416083908700390324961279 */
static const arrayDigit ga_192_gen_x1Plus[] =
	{1,3};
	/* 3 */
static const arrayDigit ga_192_gen_x1Minus[] =
	{12,25754,63413,46363,42413,24848,21836,55473,50853,40413,10264,8715,59556};
	/*  5704344264203732742656350325931731344592841761552300598426 */
static const arrayDigit ga_192_gen_plusOrder[] =
	{12,10289,46290,51633,5227,63542,39390,65535,65535,65535,65535,65535,65535};
	/* 6277101735386680763835789423176059013767194773182842284081 */
static const arrayDigit ga_192_gen_minusOrder[] =
	{13,55247,19245,13902,60308,1991,26145,0,0,0,0,0,0,1};
	/* 6277101735386680763835789423239273818400622627597807638479 =
       23 * 10864375060560251605900677743 *
            25120401793443689936479125511   */
static const arrayDigit ga_192_gen_x1OrderPlus[] =
	{12,10289,46290,51633,5227,63542,39390,65535,65535,65535,65535,65535,65535};
	/* 6277101735386680763835789423176059013767194773182842284081 */
static const arrayDigit ga_192_gen_x1OrderMinus[] =
	{12,16649,40728,9152,53911,59923,9684,22795,17096,45590,34192,25644,2849};
	/* 272917466755942641905903887966924948626114027286861201673 =
10864375060560251605900677743 * 25120401793443689936479125511
*/
static const arrayDigit ga_192_gen_x1OrderPlusRecip[] =
	{13,55247,19245,13902,60308,1993,26145,0,0,0,0,0,0,1};
static const arrayDigit ga_192_gen_lesserX1OrderRecip[] =
{12,57756,63294,44830,2517,2125,63187,65535,65535,65535,65535,65535,5887};

static const arrayDigit ga_192_gen_a[] =    {-1, 3}; /* a = -3. */
static const arrayDigit ga_192_gen_b[] =     
{12,47537,49478,57068,65208,12361,29220,59819,4007,32999,58780,1305,25633};
/* b = 2455155546008943817740293915197451784769108058161191238065. */

/***
 *** ANSI X9.62/Certicom curves
 ***/
 
/* 
 * secp192r1 
 *
 * p     = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF
 *		 = 6277101735386680763835789423207666416083908700390324961279 (d)
 * a     = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC
 *		 = 6277101735386680763835789423207666416083908700390324961276
 * b     = 64210519E59C80E70FA7E9AB72243049FEB8DEECC146B9B1
 *		 = 2455155546008943817740293915197451784769108058161191238065
 * x     = 188DA80EB03090F67CBF20EB43A18800F4FF0AFD82FF1012
 *		 = 602046282375688656758213480587526111916698976636884684818
 * y     = 07192B95FFC8DA78631011ED6B24CDD573F977A11E794811
 *		 = 174050332293622031404857552280219410364023488927386650641
 * order = FFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22831
 *		 = 6277101735386680763835789423176059013767194773182842284081
 * x1OrderRecip = 1000000000000000000000000662107c9eb94364e4b2dd7cf
 */
static const arrayDigit ga_192_secp_bp[] = 
	{12, 0xffff, 0xffff, 0xffff, 0xffff, 0xfffe, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static const arrayDigit ga_192_secp_x1Plus[] = 
	{12, 0x1012, 0x82ff, 0xafd, 0xf4ff, 0x8800, 0x43a1, 0x20eb, 0x7cbf, 0x90f6, 0xb030, 0xa80e, 0x188d};
static const arrayDigit ga_192_secp_y1Plus[] = 
	{12, 0x4811, 0x1e79, 0x77a1, 0x73f9, 0xcdd5, 0x6b24, 0x11ed, 0x6310, 0xda78, 0xffc8, 0x2b95, 0x719};
static const arrayDigit ga_192_secp_plusOrder[] = 
	{12, 0x2831, 0xb4d2, 0xc9b1, 0x146b, 0xf836, 0x99de, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
/* the curve order is prime, so x1Order = curveOrder */
static const arrayDigit ga_192_secp_x1OrderPlus[] =
	{12, 0x2831, 0xb4d2, 0xc9b1, 0x146b, 0xf836, 0x99de, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static const arrayDigit ga_192_secp_x1OrderPlusRecip[] = 
	{13, 0xd7cf, 0x4b2d, 0x364e, 0xeb94, 0x7c9, 0x6621, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1};
static const arrayDigit ga_192_secp_a[] = 
	{12, 0xfffc, 0xffff, 0xffff, 0xffff, 0xfffe, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static const arrayDigit ga_192_secp_b[] = 
	{12, 0xb9b1, 0xc146, 0xdeec, 0xfeb8, 0x3049, 0x7224, 0xe9ab, 0xfa7, 0x80e7, 0xe59c, 0x519, 0x6421};

	
/* 
 * secp256r1
 * 
 * p     = FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF
 *		 = 115792089210356248762697446949407573530086143415290314195533631308867097853951
 * a     = FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFC
 *		 = 115792089210356248762697446949407573530086143415290314195533631308867097853948
 * b     = 5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B
 *		 = 41058363725152142129326129780047268409114441015993725554835256314039467401291
 * x     = 6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296
 *		 = 48439561293906451759052585252797914202762949526041747995844080717082404635286
 * y     = 4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5
 *		 = 36134250956749795798585127919587881956611106672985015071877198253568414405109
 * order = FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
 *		 = 115792089210356248762697446949407573529996955224135760342422259061068512044369
 *                FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551
 * x1OrderRecip = 100000000fffffffffffffffeffffffff43190552df1a6c21012ffd85eedf9bfe
 */
static const arrayDigit ga_256_secp_bp[] = 
	{16, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x0, 0x1, 0x0, 0xffff, 0xffff};
static const arrayDigit ga_256_secp_x1Plus[] = 
	{16, 0xc296, 0xd898, 0x3945, 0xf4a1, 0x33a0, 0x2deb, 0x7d81, 0x7703, 0x40f2, 
	0x63a4, 0xe6e5, 0xf8bc, 0x4247, 0xe12c, 0xd1f2, 0x6b17};
static const arrayDigit ga_256_secp_y1Plus[] = 
	{16, 0x51f5, 0x37bf, 0x4068, 0xcbb6, 0x5ece, 0x6b31, 0x3357, 0x2bce, 0x9e16, 
	0x7c0f, 0xeb4a, 0x8ee7, 0x7f9b, 0xfe1a, 0x42e2, 0x4fe3};
static const arrayDigit ga_256_secp_plusOrder[] = 
	{16, 0x2551, 0xfc63, 0xcac2, 0xf3b9, 0x9e84, 0xa717, 0xfaad, 0xbce6, 0xffff, 
	0xffff, 0xffff, 0xffff, 0x0, 0x0, 0xffff, 0xffff};
static const arrayDigit ga_256_secp_x1OrderPlus[] = 
	{16, 0x2551, 0xfc63, 0xcac2, 0xf3b9, 0x9e84, 0xa717, 0xfaad, 0xbce6, 0xffff, 
	0xffff, 0xffff, 0xffff, 0x0, 0x0, 0xffff, 0xffff};
static const arrayDigit ga_256_secp_x1OrderPlusRecip[] = 
	{17, 0x9bfe, 0xeedf, 0xfd85, 0x12f, 0x6c21, 0xdf1a, 0x552, 0x4319, 0xffff, 
	0xffff, 0xfffe, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x1};
static const arrayDigit ga_256_secp_a[] = 
	{16, 0xfffc, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0, 0x0, 
	 0x0, 0x1, 0x0, 0xffff, 0xffff};
static const arrayDigit ga_256_secp_b[] = 
	{16, 0x604b, 0x27d2, 0x3c3e, 0x3bce, 0xb0f6, 0xcc53, 0x6b0, 0x651d, 0x86bc, 
	 0x7698, 0xbd55, 0xb3eb, 0x93e7, 0xaa3a, 0x35d8, 0x5ac6};

/* 
 * secp384r1
 *
 * p     = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF\
 *		   0000000000000000FFFFFFFF
 *		 = 394020061963944792122790401001436138050797392704654466679482934042457217\
 *		   71496870329047266088258938001861606973112319
 * a     = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF\
 *		   0000000000000000FFFFFFFC
 *		 = 394020061963944792122790401001436138050797392704654466679482934042457217\
 *		   71496870329047266088258938001861606973112316
 * b     = B3312FA7E23EE7E4988E056BE3F82D19181D9C6EFE8141120314088F5013875AC656398D\
 *		   8A2ED19D2A85C8EDD3EC2AEF
 *		 = 275801935599597058778490118403890480930569058563615685214287073019886892\
 *		   41309860865136260764883745107765439761230575
 * x     = AA87CA22BE8B05378EB1C71EF320AD746E1D3B628BA79B9859F741E082542A385502F25D\
 *		   BF55296C3A545E3872760AB7
 *		 = 262470350957996892686231567445669818918529234911092133878156159009255188\
 *		   54738050089022388053975719786650872476732087
 * y     = 3617DE4A96262C6F5D9E98BF9292DC29F8F41DBD289A147CE9DA3113B5F0B8C00A60B1CE\
 *		   1D7E819D7A431D7C90EA0E5F
 *		 = 832571096148902998554675128952010817928785304886131559470920590248050319\
 *		   9884419224438643760392947333078086511627871
 * order = FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC7634D81F4372DDF581A0DB2\
 *		   48B0A77AECEC196ACCC52973
 *		 = 394020061963944792122790401001436138050797392704654466679469052796276593\
 *		   99113263569398956308152294913554433653942643
 */
static const arrayDigit ga_384_secp_bp[] = 
	{24, 0xffff, 0xffff, 0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xfffe, 0xffff, 
	 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static const arrayDigit ga_384_secp_x1Plus[] = 
	{24, 0xab7, 0x7276, 0x5e38, 0x3a54, 0x296c, 0xbf55, 0xf25d, 0x5502, 0x2a38, 
	0x8254, 0x41e0, 0x59f7, 0x9b98, 0x8ba7, 0x3b62, 0x6e1d, 0xad74, 0xf320, 
	0xc71e, 0x8eb1, 0x537, 0xbe8b, 0xca22, 0xaa87};
static const arrayDigit ga_384_secp_y1Plus[] =
	{24, 0xe5f, 0x90ea, 0x1d7c, 0x7a43, 0x819d, 0x1d7e, 0xb1ce, 0xa60, 0xb8c0,
	 0xb5f0, 0x3113, 0xe9da, 0x147c, 0x289a, 0x1dbd, 0xf8f4, 0xdc29, 0x9292, 
	 0x98bf, 0x5d9e, 0x2c6f, 0x9626, 0xde4a, 0x3617};
static const arrayDigit ga_384_secp_plusOrder[] = 
	{24, 0x2973, 0xccc5, 0x196a, 0xecec, 0xa77a, 0x48b0, 0xdb2, 0x581a, 0x2ddf, 
	0xf437, 0x4d81, 0xc763, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static const arrayDigit ga_384_secp_x1OrderPlus[] = 
	{24, 0x2973, 0xccc5, 0x196a, 0xecec, 0xa77a, 0x48b0, 0xdb2, 0x581a, 0x2ddf, 
	0xf437, 0x4d81, 0xc763, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static const arrayDigit ga_384_secp_x1OrderPlusRecip[] = 
	{25, 0xd68d, 0x333a, 0xe695, 0x1313, 0x5885, 0xb74f, 0xf24d, 0xa7e5, 0xd220, 0xbc8, 
	0xb27e, 0x389c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1};
static const arrayDigit ga_384_secp_a[] = 
	{24, 0xfffc, 0xffff, 0x0, 0x0, 0x0, 0x0, 0xffff, 0xffff, 0xfffe, 0xffff, 
	 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	 0xffff, 0xffff, 0xffff, 0xffff, 0xffff};
static const arrayDigit ga_384_secp_b[] = 
	{24, 0x2aef, 0xd3ec, 0xc8ed, 0x2a85, 0xd19d, 0x8a2e, 0x398d, 0xc656, 0x875a, 
	 0x5013, 0x88f, 0x314, 0x4112, 0xfe81, 0x9c6e, 0x181d, 0x2d19, 0xe3f8, 0x56b, 
	 0x988e, 0xe7e4, 0xe23e, 0x2fa7, 0xb331};

/*
 * secp521r1
 * p     = 01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\
 *		   FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
 *		 = 686479766013060971498190079908139321726943530014330540939446345918554318\
 *		   339765605212255964066145455497729631139148085803712198799971664381257402\
 *		   8291115057151
 * a     = 01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\
 *		   FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC
 *		 = 686479766013060971498190079908139321726943530014330540939446345918554318\
 *		   339765605212255964066145455497729631139148085803712198799971664381257402\
 *		   8291115057148
 * b     = 0051953EB9618E1C9A1F929A21A0B68540EEA2DA725B99B315F3B8B489918EF109E15619\
 *		   3951EC7E937B1652C0BD3BB1BF073573DF883D2C34F1EF451FD46B503F00
 *		 = 109384903807373427451111239076680556993620759895168374899458639449595311\
 *		   615073501601370873757375962324859213229670631330943845253159101291214232\
 *		   7488478985984
 * x     = 00C6858E06B70404E9CD9E3ECB662395B4429C648139053FB521F828AF606B4D3DBAA14B\
 *		   5E77EFE75928FE1DC127A2FFA8DE3348B3C1856A429BF97E7E31C2E5BD66
 *		 = 266174080205021706322876871672336096072985916875697314770667136841880294\
 *		   499642780849154508062777190235209424122506555866215711354557091681416163\
 *		   7315895999846
 * y     = 011839296A789A3BC0045C8A5FB42C7D1BD998F54449579B446817AFBD17273E662C97EE\
 *		   72995EF42640C550B9013FAD0761353C7086A272C24088BE94769FD16650
 *		 = 375718002577002046354550722449118360359445513476976248669456777961554447\
 *		   744055631669123440501294553956214444453728942852258566672919658081012434\
 *		   4277578376784
 * order = 01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFA5186\
 *		   8783BF2F966B7FCC0148F709A5D03BB5C9B8899C47AEBB6FB71E91386409
 *		 = 686479766013060971498190079908139321726943530014330540939446345918554318\
 *		   339765539424505774633321719753296399637136332111386476861244038034037280\
 *		   8892707005449
 * orderRecip = 200 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000005 \
 *					ae79787c 40d06994 8033feb7 08f65a2f c44a3647 7663b851 449048e1 6ec79bf7
 * orderRecip = 2000000000000000000000000000000000000000000000000000000000000000005ae79787c40d069948033feb708f65a2fc44a36477663b851449048e16ec79bf7
 */
static const arrayDigit ga_521_secp_bp[] = 
	{33, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
	 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	 0xffff, 0xffff, 0xffff, 0x1ff};
static const arrayDigit ga_521_secp_x1Plus[] = 
	{33, 0xbd66, 0xc2e5, 0x7e31, 0xf97e, 0x429b, 0x856a, 0xb3c1, 0x3348, 0xa8de, 0xa2ff, 
	0xc127, 0xfe1d, 0x5928, 0xefe7, 0x5e77, 0xa14b, 0x3dba, 0x6b4d, 0xaf60, 0xf828, 0xb521, 
	0x53f, 0x8139, 0x9c64, 0xb442, 0x2395, 0xcb66, 0x9e3e, 0xe9cd, 0x404, 0x6b7, 0x858e, 0xc6};
static const arrayDigit ga_521_secp_y1Plus[] = 
	{33, 0x6650, 0x9fd1, 0x9476, 0x88be, 0xc240, 0xa272, 0x7086, 0x353c, 0x761, 0x3fad,
	 0xb901, 0xc550, 0x2640, 0x5ef4, 0x7299, 0x97ee, 0x662c, 0x273e, 0xbd17, 0x17af, 0x4468, 
	 0x579b, 0x4449, 0x98f5, 0x1bd9, 0x2c7d, 0x5fb4, 0x5c8a, 0xc004, 0x9a3b, 0x6a78, 0x3929, 
	 0x118};
static const arrayDigit ga_521_secp_plusOrder[] = 
	{33, 0x6409, 0x9138, 0xb71e, 0xbb6f, 0x47ae, 0x899c, 0xc9b8, 0x3bb5, 0xa5d0, 0xf709, 
	0x148, 0x7fcc, 0x966b, 0xbf2f, 0x8783, 0x5186, 0xfffa, 0xffff, 0xffff, 0xffff, 0xffff, 
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	0x1ff};
static const arrayDigit ga_521_secp_x1OrderPlus[] = 
	{33, 0x6409, 0x9138, 0xb71e, 0xbb6f, 0x47ae, 0x899c, 0xc9b8, 0x3bb5, 0xa5d0, 0xf709, 
	0x148, 0x7fcc, 0x966b, 0xbf2f, 0x8783, 0x5186, 0xfffa, 0xffff, 0xffff, 0xffff, 0xffff, 
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	0x1ff};
static const arrayDigit ga_521_secp_x1OrderPlusRecip[] =
{33, 0x9bf7, 0x6ec7, 0x48e1, 0x4490, 0xb851, 0x7663, 0x3647, 0xc44a, 0x5a2f, 0x8f6, 0xfeb7, 0x8033, 0x6994, 0x40d0, 0x787c, 0xae79, 0x5, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x200}; 
static const arrayDigit ga_521_secp_a[] = 
	{33, 0xfffc, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 
	0xffff, 0xffff, 0xffff, 0x1ff};
static const arrayDigit ga_521_secp_b[] = 
	{33, 0x3f00, 0x6b50, 0x1fd4, 0xef45, 0x34f1, 0x3d2c, 0xdf88, 0x3573, 0xbf07, 
	0x3bb1, 0xc0bd, 0x1652, 0x937b, 0xec7e, 0x3951, 0x5619, 0x9e1, 0x8ef1, 0x8991, 
	0xb8b4, 0x15f3, 0x99b3, 0x725b, 0xa2da, 0x40ee, 0xb685, 0x21a0, 0x929a, 0x9a1f, 
	0x8e1c, 0xb961, 0x953e, 0x51};
