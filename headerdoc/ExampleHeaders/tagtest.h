/*! @header
	@abstract Abstract tag.
	@discussion
		This is a header discussion.

        	This header attempts to test every HeaderDoc tag (except 
		\@framework, \@frameworkuid, \@frameworkpath, and
		\@frameworkcopyright).

		This is a link: @link test_every_tag_internal_var @/link.  This
		is also a link: {@link test_every_tag_internal_var}.

	@important
		This header's classes are hopelessly broken.

	Yup.  Hopelessly broken.

	@warning
		This stuff doesn't really work.

	No, really.  It doesn't.

	@attribute foo
		Description for attribute foo.

	@attributeblock My custom attribute block
		Descripton for my custom attribute block.

	@attributelist List name
		Term Definition for Term
		Term2 Definition for Term 2.
		Term3 Definition for Term 2.

	@author Alfred E. Neuman

	@availability
		Coming in June to a theater near you.

	@CFBundleIdentifier
		com.myfavoritetvshowgotcancelled

	@charset iso-8859-1

	@compilerflag
		Always pass -Ltest_every_tag when compiling this mess.

	@copyright
		Copyright &copy; 2010 Somebody.  All Wrongs Reversed.

	@dependency
		This depends on the phase of the moon.

	@deprecated
		It definitely should be.

	@encoding UTF-8

	@flag
		What the heck does this do?

	@headerpath
		/Library/Headers/foo.h

	@ignore test_every_tag_ignore_this_token
	@ignorefuncmacro test_every_tag_ignore_this_funcmacro

	@indexgroup My index group

	@meta foo bar

	@namespace MyNamespace

	@preprocinfo
		This header doesn't do anything fun with C preprocessing.

	@performance
		The perormance is terrible.

	@related
		Related information goes here.

	@security
		This is discussion of security considerations.

	@see test_every_tag_enum test_every_tag_enum

	@seealso test_every_tag_class_2 test_every_tag_class_2


	@since
		Available since 2001.

	@unsorted

	@updated
		01-01-2001

	@version
		One version newer than the last version you're compatible with.


	@whyinclude
		Honestly, we're not really sure, either.


	@apiuid //apple_ref/doc/test_every_tag

 */


/*! @group Doxygen-style tags and second-tier const/constant tags */
/*!
	@brief
		This is the abstract.

		This is the discussion.
	@const test_every_tag_const_a
		Description for test_every_tag_const_a.
	@constant test_every_tag_const_b
		Description for test_every_tag_const_a.
 */
typedef enum test_every_tag_enum {
	test_every_tag_const_a = 3,
	test_every_tag_const_b
};

/*! @group Test of \@callback */
/*!
	@discussion
		Test.
	@field test_every_tag_field
		Description for test_every_tag_field.
	@callback test_every_tag_cb
		Description for test_every_tag_cb.
 */
typedef struct test_every_tag_struct {
	int (*test_every_tag_cb)(int a);
	char *test_every_tag_field;
} test_every_tag_td;

/*! @group Test of \@callback */
/*!
	@discussion
		Test.
	@unformatted
	@field test_every_tag_field_2
		Description for test_every_tag_field.
	@callback test_every_tag_cb_2
		Description for test_every_tag_cb.
 */
typedef struct test_every_tag_struct_2 {
	int (*test_every_tag_cb_2)(int a);
	char *test_every_tag_field_2;
} test_every_tag_td_2;

/*! @group Test of class-specific stuff. */
/*!
	@attribute foo
		Description for attribute foo.

	@attributeblock My custom attribute block
		Descripton for my custom attribute block.

	@attributelist List name
		Term Definition for Term
		Term2 Definition for Term 2.
		Term3 Definition for Term 2.

	@author Alfred E. Neuman

	@availability
		Coming in June to a theater near you.

	@dependency
		This depends on the phase of the moon.

	@deprecated
		It definitely should be.

	@indexgroup My index group

	@namespace MyNamespace

	@performance
		The perormance is terrible.

	@security
		This is discussion of security considerations.

	@see test_every_tag_enum test_every_tag_enum

	@seealso test_every_tag_class_2 test_every_tag_class_2

	@since
		Available since 2001.

	@unsorted

	@updated
		01-01-2001

	@version
		One version newer than the last version you're compatible with.

	@apiuid //apple_ref/doc/test_every_tag_class

	@description
		Test class.  Wait, does this tag really work?

	@important
		This header's classes are hopelessly broken.

	Yup.  Hopelessly broken.

	@warning
		This stuff doesn't really work.

	No, really.  It doesn't.

	@alsoinclude
		test_every_tag_function_3

	@coclass
		Uses test_every_tag_class_2 class.

	@instancesize
		Freaking huge.

	@classdesign
		This class was grown organically, not designed.

	@exception
		Throws foo_exception.

	@helperclass test_every_tag_class_2

	@helps test_every_tag_class_2

	@vargroup
		Group of local variables.

	@var local_1
		Local variable #1.

	@var local_2
		Local variable #2.

	@templatefield foo
		It's the foo.

	@ownership
		I don't remember what this is for, either.

	@serial
		This is the \@serial field.
	@serialfield
		fieldname int This is the \@serialfield field.

	@super explicit_superclass_foo

 */
class test_every_tag_class_1<foo>
{
	/*! Hmm. */
	int k;
};

/*!
	@abstract Another one.
	@helper test_every_tag_class_1
	@throws
		Throws bar_exception.
	@superclass explicit_superclass_bar
 */
class test_every_tag_class_2
{
	/*! Hmm. */
	int k;

	/*! @abstract No abstract.
	    @serialData
		This is the \@serialdata field.
	 */
	int foo(char *bar);
};


/*!
	@abstract One more.
	@details
		Testing the \@details tag.
 */
int test_every_tag_var;

/*! @definedblock test_every_tag_multi_define
	The members of this define block shouldn't show up.
    @hidesingletons
    @noParse
 */
#define test_every_tag_define_1 VALUE_1
#define test_every_tag_define_2 VALUE_2
#define test_every_tag_define_3 VALUE_3
/*! @/definedblock */


/*! @defineblock test_every_tag_multi_define_2
	The members of this define block shouldn't show up.
    @hidesingletons
 */
#define test_every_tag_define_4 VALUE_4
/*! @define test_every_tag_define_5
	Subdefine
 */
#define test_every_tag_define_5 VALUE_5
/*! @defined test_every_tag_define_6
	Subdefine
 */
#define test_every_tag_define_6 VALUE_6
/*! @/defineblock */

/*! @define test_every_tag_define_7
	The contents should be hidden.
    @hidecontents
 */
#define test_every_tag_define_7 VALUE_7

/*! @defined test_every_tag_define_8
	Discussion for test_every_tag_define_8.
    @parseOnly
 */
#define test_every_tag_define_8 VALUE_8


/*!
	@function test_every_tag_function
		Description for test_every_tag_function.

	@return
		This is the return info.
	@param parm_1
		Description for parm_1.
 */
int test_every_tag_function(char *parm_1);

/*!
	@function test_every_tag_function_2
		Description for test_every_tag_function_2.

	@returns
		This is the return info.
	@param parm_1
		Description for parm_1.
 */
int test_every_tag_function_2(char *parm_1);

/*!
	@function test_every_tag_function_3
		Description for test_every_tag_function_3.

	@result
		This is the return info.
	@param parm_1
		Description for parm_1.
 */
int test_every_tag_function_3(char *parm_1);

/*!
	This is a test of an internal var.
	@internal
 */
int test_every_tag_internal_var;

/*!
	@name group name in a weird way.
 */

/*!
        @abstract Abstract tag.
        @discussion
                This is a function discussion.

                This function attempts to test every HeaderDoc tag (except
                \@framework, \@frameworkuid, \@frameworkpath, and
                \@frameworkcopyright).

                This is a link: @link test_every_tag_internal_var @/link.  This
                is also a link: {@link test_every_tag_internal_var}.

        @important
                This function's classes are hopelessly broken.

        Yup.  Hopelessly broken.

        @warning
                This stuff doesn't really work.

        No, really.  It doesn't.

	@attribute foo
		Description for attribute foo.

	@attributeblock My custom attribute block
		Descripton for my custom attribute block.

	@attributelist List name
		Term Definition for Term
		Term2 Definition for Term 2.
		Term3 Definition for Term 2.

	@author Alfred E. Neuman

	@availability
		Coming in June to a theater near you.

	@deprecated
		It definitely should be.

	@indexgroup My index group

	@namespace MyNamespace

	@performance
		The perormance is terrible.

	@security
		This is discussion of security considerations.

	@see test_every_tag_enum test_every_tag_enum

	@seealso test_every_tag_class_2 test_every_tag_class_2

	@since
		Available since 2001.

	@updated
		01-01-2001

	@version
		One version newer than the last version you're compatible with.

	@apiuid //apple_ref/doc/test_every_tag_function

	@description
		Test class.  Wait, does this tag really work?

	@important
		This header's classes are hopelessly broken.

	Yup.  Hopelessly broken.

	@warning
		This stuff doesn't really work.

	No, really.  It doesn't.

	@exception
		Throws foo_exception.

	@vargroup
		Group of local variables.

	@var local_1
		Local variable #1.

	@var local_2
		Local variable #2.

	@templatefield foo
		It's the foo.

	@serial
		This is the \@serial field.
	@serialfield
		fieldname int This is the \@serialfield field.

	@param q
		Parameter q.
	@returns
		Returns something.
 */
int every_tag_in_a_function(char *q);

/*! Ugh */
int test_every_tag_internal_var_2;



/* To do:


TOP LEVEL:

availabilitymacro
category
class
const(ant)?
enum
file
function
functiongroup
group
header
interface
language
method
methodgroup
property
protocol
struct
template
typedef
union
var

 */
