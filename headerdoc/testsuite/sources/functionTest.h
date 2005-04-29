/*! @header Function.h
    @abstract This header tests the supported types of functions declarations.  
    @discussion
	While this header is important for parser testing purposes, a
	better example of how to document functions can be found in fgroup.h.
*/

/*! @function anonymous_parameter_test
    @abstract Test of anonymous parameters (void *, void *).
 */
    virtual IOReturn anonymous_parameter_test(void *command, void *data, void *, void *);

/*! @function varargs_test
    @abstract Test of variable argument lists.
    @param foo Format string.
    @param ... additional parameters.
*/
    virtual IOReturn varargs_test(const char *foo,...);

/*! @function arrayparam_test
    @param someArray This is a test of array parameters.
 */
    void testFunc(char someArray[32]);

