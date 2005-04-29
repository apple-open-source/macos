
/*! @class myclass
	This is a class test.
 */
class otherclass;
class myclass
{
    /*! @function func This is a test functon */
    char *func(int a);

}

/*! @class classC
	This should document just the line that follows.
 */
class classC;
class ThisShouldNeverAppear
{
    /*! @function RootLevel
	This should (counterintuitively) show up as an ordinary C function,
	since the enclosing class is not marked up....
     */
    char *RootLevel(int a);

}

