/*! @header
    @updated 2999-10-10
 */

/*! @class Bar
    @discussion This is a test class.
    @updated 2003-04-01
*/
class Bar {
    public:
    /*! @functiongroup group_1 */
    /*!	@function foo
	@abstract Test method
     */
    static int foo(int a, char *b) { if (a < 3) printf("%s\n", b); };
    /*! @functiongroup group_2 */
    /*! @method mymethod
	@abstract My method
      */
    void mymethod(int b);

    /*! @struct foobar
	@abstract test foobar
        @updated 2003-02-01
     */
    struct {
	int a;
	char *b;
    } foobar;

    /*! @var test
        @abstract test
	@updated 12-14-06
     */
    int test;
    /*! @functiongroup group_1 */
    /*! @function group1_test
     */
    void group1_test(int a);

private:

/*!
    @var foovar
    @abstract This is a test abstract.
    @discussion This is a test discussion.
    @updated 2003-05-02
 */
int foovar;
} ;

/*! @template correlate
    @templatefield FOO this is the base data type
    @templatefield BAR this is the data type of correlated data
    @discussion This class does basic correlation of two data types
*/
template <class FOO, BAR>
class correlate
{
    public:
	/*! @function insert
	    @discussion insert new entry
	 */
	bool insert(FOO, BAR);

	/*! @function commatest
	    @abstract Watch for extra line breaks
	 */
	virtual bool CreateItemWithIdentifier(const ACFString &inIdentifier,
		const AEventParaneter<CFTypeRef> &inConfigData,
		AEventParameter<HIToolbarItemRef,AWriteOnly> &outItem);

	/*! @function S3Handle
	    @discussion Hmm.
	    @templatefield FOO the foo type.
	 */
	template<class FOO> operator S3Handle<FOO>() { return S3Handle<S2>(mData); };
}
/*! @class test
    @discussion This had better not disappear.
*/
class test
{
    public:
	int testint;
}
