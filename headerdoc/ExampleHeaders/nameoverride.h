/*! @header OverrideHeaderName.h
    @abstract This tests header name override.
    @discussion
	If this shows up as "OverrideHeaderName.h", things worked.  If it shows up as
	"nameoverride.h", then something is wrong.
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

