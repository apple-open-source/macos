/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.resource;

/**
 * This is the root exception for the exception hierarchy defined for
 * the connector architecture.
 *
 * A ResourceException contains three items, the first two of which are
 * set from the constructor.  The first is a standard message string
 * which is accessed via the getMessage() method.  The second is an
 * errorCode which is accessed via the getErrorCode() method.  The third
 * is a linked exception which provides more information from a lower
 * level in the resource manager.  Linked exceptions are accessed via
 * get/setLinkedException.
 */

public class ResourceException extends Exception {
    private String errorCode = null;
    private Exception linkedException = null;

    /**
     * Create an exception with a reason.
     */
    public ResourceException( String reason ) {
        super( reason );
    }

    /**
     * Create an exception with a reason and an errorCode.
     */
    public ResourceException( String reason, String errorCode ) {
        super( reason );
	this.errorCode = errorCode;
    }

    /**
     * Return the error code.
     */
    public String getErrorCode() {
        return errorCode;
    }

    /**
     * Set a linked exception.
     */
    public void setLinkedException( Exception linkedException ) {
        this.linkedException = linkedException;
    }

    /**
     * Get any linked exception.
     */
    public Exception getLinkedException() {
        return linkedException;
    }
}
