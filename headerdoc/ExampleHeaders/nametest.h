
/*! @header
	Test Header for name tagging edge cases.

 */

/*!
	This really shouldn't be that hard....
	Should be called "simple".
 */
typedef uint32_t simple;


/*! @enum test
	This is a horrible, evil test case.
	Should have test and an enum linked together with both
	called "test".
	@constant kTest1 Is 3
	@constant kTest2 Is 4
 */
typedef uint32_t test;

enum {
	kTest1 = 3,
	kTest2 = 4
};

/*! @enum multi word anonymous enum & stuff
    @discussion Should be called "multi word anonymous enum & stuff"
    @constant kFoo1 Is 3
 */
enum {
	kFoo1 = 3
};

/*! @enum foo
	Should be called "foo".
	@constant kFoo1 Is 3
	@constant doesnotexist Is bogus
 */
enum {
	kFoo1 = 3
} foo;


/*! this should be called foostruct. */
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

/*! This should be a typedef with two outer names and a tag name.
    If the "outer names only" flag is set, it should show
    up as ono1 and ono2.  If that flag is not set, it should
    also show up as iname.
 */
typedef struct iname {
	char *b;
} ono1, ono2;

/*! @defineblock My define block
	This is a terriffic define block.  Should be called My define block
	with separate definitions for def_1 and def_2.
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


/*! @function reverseorder
	Should be called reverseorder and not_the_right_function.

	The apple_ref should be
	//apple_ref/doc/title:func/reverseorder and eventually
	//apple_ref/c/func/not_the_right_function
 */
int not_the_right_function(char *k);

/*! @function reverseorder 
    Tests to see if apple_ref disambiguation works if
    a broken doc ref appears first.

    Should be called reverseorder.  The apple_ref should be
    //apple_ref/c/func/reverseorder
 */
int reverseorder(char *k);

/*! @function normalorder
    Should be called normalorder.  The apple_ref should be
    //apple_ref/c/func/normalorder
 */
int normalorder(char *);

/*! @function normalorder
    Should be called normalorder and wrong_function_again.
    The apple_ref should be
    //apple_ref/doc/title:func/normalorder and eventually
    //apple_ref/c/func/wrong_function_again
 */
int wrong_function_again(char *);

/*! @define test */
#define test foo::test

/*! class should be called foo_t */
class foo_t
{
/*! @function testfunc
	@param blah This is blah.
*/
int testfunc(struct test &blah);

/*! operator should be named "operator &". */
foo_t operator &(foo_t &a);

};

/*! @typedef teststruct_t
    @field field_1 field 1
 */
typedef struct teststruct {
	int field_1;
} teststruct_t;


/*! @struct evil multi-word struct.
	This should be part of the discussion here.
	@discussion This should also be discussion.
 */
struct evil;

/*! @struct single_line_disc single-line discussion goes here.
 */
struct single_line_disc;

/*! @var values
    @abstract This should only be called values, not MAX_PERMUTATION_SIZE.
*/
unsigned int values[MAX_PERMUTATION_SIZE];

