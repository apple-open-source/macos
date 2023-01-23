/**************************************************************
 *
 *	giants.h
 *
 *	Header file for large-integer arithmetic library giants.c.
 *
 *	Updates:
 *          18 Jul 99  REC  Added fer_mod().
 *          30 Apr 98  JF   USE_ASSEMBLER_MUL removed
 *          29 Apr 98  JF   Function prototypes cleaned up
 *			20 Apr 97  RDW
 *
 *	c. 1997 Perfectly Scientific, Inc.
 *	All Rights Reserved.
 *
 **************************************************************/


/**************************************************************
 *
 * Error Codes
 *
 **************************************************************/

#define DIVIDEBYZERO  1
#define OVFLOW      2
#define SIGN	      3
#define OVERRANGE     4
#define AUTO_MUL 0
#define GRAMMAR_MUL 1
#define FFT_MUL 2
#define KARAT_MUL 3

/**************************************************************
 *
 * Preprocessor definitions
 *
 **************************************************************/

/* 2^(16*MAX_SHORTS)-1 will fit into a giant, but take care:
 * one usually has squares, etc. of giants involved, and
 * every intermediate giant in a calculation must fit into
 * this many shorts. Thus, if you want systematically to effect
 * arithmetic on B-bit operands, you need MAX_SHORTS > B/8,
 * perferably a tad larger than this; e.g. MAX_SHORTS > B/7.
 */
#define MAX_SHORTS (1<<19)	

#define INFINITY (-1)
#define FA 0
#define TR 1
#define COLUMNWIDTH 64

#define TWOPI (double)(2*3.1415926535897932384626433)
#define SQRT2 (double)(1.414213562373095048801688724209)
#define SQRTHALF (double)(0.707106781186547524400844362104)
#define TWO16 (double)(65536.0)
#define TWOM16 (double)(0.0000152587890625)

/* Decimal digit ceiling in digit-input routines. */
#define MAX_DIGITS 10000

/* Next, mumber of shorts per operand 
   at which Karatsuba breaks over. */
#define KARAT_BREAK 40

/* Next, mumber of shorts per operand 
   at which FFT breaks over. */
#define FFT_BREAK 200

#define newmin(a,b) ((a)<(b)? (a) : (b))
#define newmax(a,b) ((a)>(b)? (a) : (b))

/* Maximum number of recursive steps needed to calculate
 * gcds of integers. */
#define STEPS 32

/* The limit below which hgcd is too ponderous */
#define GCDLIMIT 5000

/* The limit below which ordinary ints will be used */
#define INTLIMIT  31

/* Size by which to increment the stack used in pushg() and popg(). */
#define	STACK_GROW	16

#define gin(x)   gread(x,stdin)
#define gout(x)  gwriteln(x,stdout)


/**************************************************************
 *
 * Structure definitions
 *
 **************************************************************/

typedef struct
{
	 int 					sign;
	 unsigned short	n[1];       /* number of shorts = abs(sign) */
} giantstruct;

typedef giantstruct *giant;

typedef struct _matrix
{
	 giant 				ul;			/* upper left */
	 giant				ur;         /* upper right */
	 giant				ll;         /* lower left */
	 giant				lr;       	/* lower right */
} *gmatrix;

typedef struct
{
	double				re;
	double				im;
} complex;


/**************************************************************
 *
 * Function Prototypes
 *
 **************************************************************/

/**************************************************************
 *
 * Initialization and utility functions
 *
 **************************************************************/

/* trig lookups. */
void			init_sinCos(int);
double			s_sin(int);
double			s_cos(int);


/* Creates a new giant, numshorts = INFINITY invokes the
 * maximum MAX_SHORTS. */
giant 			newgiant(int numshorts);

/* Creates a new giant matrix, but does not malloc
 * the component giants. */
gmatrix			newgmatrix(void);

/* Returns the bit-length n; e.g. n=7 returns 3. */
int 			bitlen(giant n);

/* Returns the value of the pos bit of n. */
int 			bitval(giant n, int pos);

/* Returns whether g is one. */
int 			isone(giant g);

/* Returns whether g is zero. */
int 			isZero(giant g);

/* Copies one giant to another. */
void 			gtog(giant src, giant dest);

/* Integer <-> giant. */
void 			itog(int n, giant g);
signed int		gtoi (giant);

/* Returns the sign of g: -1, 0, 1. */
int 			gsign(giant g);

/* Returns 1, 0, -1 as a>b, a=b, a<b. */
int 			gcompg(giant a, giant b);

/* Set AUTO_MUL for automatic FFT crossover (this is the
 * default), set FFT_MUL for forced FFT multiply, set
 * GRAMMAR_MUL for forced grammar school multiply. */
void 		setmulmode(int mode);

/**************************************************************
 *
 * I/O Routines
 *
 **************************************************************/

/* Output the giant in decimal, with optional newlines.  */
void 		gwrite(giant g, FILE *fp, int newlines);

/* Output the giant in decimal, with both '\'-newline
 * notation and a final newline. */
void 		gwriteln(giant g, FILE *fp);

/* Input the giant in decimal, assuming the formatting of
 * 'gwriteln'. */
void 		gread(giant g, FILE *fp);

/**************************************************************
 *
 * Math Functions
 *
 **************************************************************/

/* g := -g. */
void 		negg(giant g);

/* g := |g|. */
void 		absg(giant g);

/* g += i, with i non-negative and < 2^16. */
void 		iaddg(int i,giant g);

/* b += a. */
void 		addg(giant a, giant b);

/* b -= a. */
void 		subg(giant a, giant b);

/* Returns the number of trailing zero bits in g. */
int			numtrailzeros(giant g);

/* u becomes greatest power of two not exceeding u/v. */
void		bdivg(giant v, giant u);

/* Same as invg, but uses bdivg. */
int 		binvg(giant n, giant x);

/* If 1/x exists (mod n), 1 is returned and x := 1/x.  If
 * inverse does not exist, 0 is returned and x := GCD(n, x). */
int 		invg(giant n, giant x);

int			mersenneinvg(int q, giant x);

/* Classical GCD, x:= GCD(n, x). */
void 		cgcdg(giant n, giant x);

/* General GCD, x:= GCD(n, x). */
void 		gcdg(giant n, giant x);

/* Binary GCD, x:= GCD(n, x). */
void 		bgcdg(giant n, giant x);

/* g := m^n, no mod is performed. */
void 		powerg(int a, int b, giant g);

/* r becomes the steady-state reciprocal 2^(2b)/d, where
 * b = bit-length of d-1. */
void		make_recip(giant d, giant r);

/* n := [n/d], d positive, using stored reciprocal directly. */
void		divg_via_recip(giant d, giant r, giant n);

/* n := n % d, d positive, using stored reciprocal directly. */
void		modg_via_recip(giant d, giant r, giant n);

/* num := num % den, any positive den. */
void 		modg(giant den, giant num);

/* a becomes (a*b) (mod 2^q-k) where q % 16 == 0 and k is "small"
 * (0 < k < 65535). Returns 0 if unsuccessful, otherwise 1. */
int			feemulmod(giant x, giant y, int q, int k);

/* g := g/n, and (g mod n) is returned. */
int 		idivg(int n, giant g);

/* num := [num/den], any positive den. */
void 		divg(giant den, giant num);

/* num := [num/den], any positive den. */
void 		powermod(giant x, int n, giant z);

/* x := x^n (mod z). */
void 		powermodg(giant x, giant n, giant z);

/* x := x^n (mod 2^q+1). */
void 		fermatpowermod(giant x, int n, int q);

/* x := x^n (mod 2^q+1). */
void 		fermatpowermodg(giant x, giant n, int q);

/* x := x^n (mod 2^q-1). */
void 		mersennepowermod(giant x, int n, int q);

/* x := x^n (mod 2^q-1). */
void 		mersennepowermodg(giant x, giant n, int q);

/* Shift g left by bits, introducing zeros on the right. */
void 		gshiftleft(int bits, giant g);

/* Shift g right by bits, losing bits on the right. */
void 		gshiftright(int bits, giant g);

/* dest becomes lowermost n bits of src.
 * Equivalent to dest = src % 2^n. */
void 		extractbits(int n, giant src, giant dest);

/* negate g. g is mod 2^n+1. */
void 		fermatnegate(int n, giant g);

/* g := g (mod 2^n-1). */
void 		mersennemod(int n, giant g);

/* g := g (mod 2^n+1). */
void 		fermatmod(int n, giant g);

/* g := g (mod 2^n+1). */
void 		fer_mod(int n, giant g, giant modulus);

/* g *= s. */
void 		smulg(unsigned short s, giant g);

/* g *= g. */
void 		squareg(giant g);

/* b *= a. */
void 		mulg(giant a, giant b);

/* A giant gcd.  Modifies its arguments. */
void		ggcd(giant xx, giant yy);
