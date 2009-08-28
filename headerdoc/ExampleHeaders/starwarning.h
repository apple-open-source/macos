/*!
    @discussion
 * This should generate a warning
 * in paranoid mode.
 */
int somestarspresent;

/*! @discussion
    This shouldn't generate any
    warning even in paranoid mode.
 */
char *nostarspresent;

/*! @discussion
 *  Neither should this
 *  (even in paranoid mode).
 */
void *allstarspresent;

/*! @discussion
 *  This contains a text block.
 * 
 *  @textblock
	This is sacrosanct.
    @/textblock
 * 
 *  The comment continues here.
 */
int allstarsexcepttextblock;


/*! @discussion
 *  This also contains a text block.
 * 
 *  @textblock
 *	This is sacrosanct except the leading stars.
 *  @/textblock
 * 
 *  The comment continues here.
 */
int allstarsincludingttextblock;

/*! @discussion
 *  This also contains a text block.
 * 
 *  @textblock
 *	This is sacrosanct including the star on
        the previous line and the next line.
 *  @/textblock
 * 
 *  The comment continues here.
 */
int allstarsexceptpartoftextblock;


/*! This is a class test. */
class star_test_class
{

/*!
    @discussion
 * This should generate a warning
 * in paranoid mode.
 */
int somestarspresent;

/*! @discussion
    This shouldn't generate any
    warning even in paranoid mode.
 */
char *nostarspresent;

/*! @discussion
 *  Neither should this
 *  (even in paranoid mode).
 */
void *allstarspresent;

/*! @discussion
 *  This contains a text block.
 * 
 *  @textblock
	This is sacrosanct.
    @/textblock
 * 
 *  The comment continues here.
 */
int allstarsexcepttextblock;


/*! @discussion
 *  This also contains a text block.
 * 
 *  @textblock
 *	This is sacrosanct except the leading stars.
 *  @/textblock
 * 
 *  The comment continues here.
 */
int allstarsincludingttextblock;

/*! @discussion
 *  This also contains a text block.
 * 
 *  @textblock
 *	This is sacrosanct including the star on
        the previous line and the next line.
 *  @/textblock
 * 
 *  The comment continues here.
 */
int allstarsexceptpartoftextblock;


};
