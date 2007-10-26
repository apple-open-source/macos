
/*! @header
	Test Header for name tagging edge cases.

 */

/*!
	This really shouldn't be that hard....
 */
typedef uint32_t simple;


/*! @enum test
	This is a horrible, evil test case.
	@constant kTest1 Is 3
	@constant kTest2 Is 4
 */
typedef uint32_t test;

enum {
	kTest1 = 3,
	kTest2 = 4
};

/*! @enum multi word anonymous enum & stuff
    @discussion
    @constant kFoo1 Is 3
 */
enum {
	kFoo1 = 3
};

/*! @enum foo
	@constant kFoo1 Is 3
	@constant doesnotexist Is bogus
 */
enum {
	kFoo1 = 3
} foo;


/*! this should be foostruct. */
struct foostruct;

/*! @typedef foo_td
	This is the foo typedef and associated foo_td_struct.

	Note that the foo_td_struct should NOT appear in the
	Structs section as a foo_td entry.  It should appear
	only as a foo_td_struct entry under structs and as
	a foo_td entry under typedefs.
	@abstract foo typedef abstract here.
 */
struct foo_td_struct {
	int a;
};
typedef struct foo_td_struct *foo_td;

/*! @function CFStringRef foo_func
	This is foo_func, but should also be called
	CFStringRef foo_func.
 */
CFStringRef foo_func(int a);

/*! This should be a typedef with multiple tag names.
    If the "outer names only" flag is set, it should show
    up as ono1 and ono2.  If that flag is not set, it should
    show up as iname.
 */
typedef struct iname {
	char *b;
} ono1, ono2;

/*! @defineblock My define block
	This is a terriffic define block.
 */
/*! Define 1. */
#define def_1 foo

/*! Define 2. */
#define def_2 bar

/*! @/defineblock */

    /*! @var req
      Tracks the current common memory reservations */
    win_req_t                   req[CISTPL_MEM_MAX_WIN];

/*! @define def_3_altname This should be called def_3 and def_3_altname */
#define def_3 baz


/*! @function reverseorder */
int not_the_right_function(char *k);

/*! @function reverseorder 
    Tests to see if apple_ref disambiguation works if
    A broken doc ref appears first.
 */
int reverseorder(char *k);

/*! @function normalorder */
int normalorder(char *);

/*! @function normalorder */
int wrong_function_again(char *);

/*! @define test */
#define test foo::test

/*! class */
class foo_t
{
/*! @function testfunc
	@param blah This is blah.
*/
int testfunc(struct test &blah);

/*! operator */
foo_t operator &(foo_t &a);

};

/*! @typedef teststruct_t
    @field field_1 field 1
 */
typedef struct teststruct {
	int field_1;
} teststruct_t;


/*! @struct evil multi-word struct.
	This should be the discussion here.
	@discussion This should also be discussion.
 */
struct evil;

/*! @struct single_line_disc single-line discussion goes here.
 */
struct single_line_disc;
