
package javax.security.auth.callback;

public abstract interface CallbackHandler
{

    public void invokeCallback(Callback[] callbacks)
	throws java.io.IOException, UnsupportedCallbackException;
}
