/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  The NotSupportedException exception indicates that an operation is not
 *  supported.
 *
 *  For example, the {@link TransactionManager#begin()} and 
 *  {@link UserTransaction#begin()} methods throw this exception if the
 *  calling thread is already associated with a transaction, and nested
 *  transactions are not supported.
 *
 *  @version $Revision: 1.2 $
 */
public class NotSupportedException extends Exception
{

    /**
     *  Creates a new <code>NotSupportedException</code> without a
     *  detail message.
     */
    public NotSupportedException()
    {
    }

    /**
     *  Constructs an <code>NotSupportedException</code> with the specified
     *  detail message.
     *
     *  @param msg the detail message.
     */
    public NotSupportedException(String msg)
    {
        super(msg);
    }
}
