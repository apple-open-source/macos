/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

import java.rmi.RemoteException;

/**
 *  This exception indicates that a remote invocation request carried a null
 *  transaction context, but that an active transaction context was needed.
 *
 *  @version $Revision: 1.2 $
 */
public class TransactionRequiredException extends RemoteException
{

    /**
     *  Creates a new <code>TransactionRequiredException</code> without a
     *  detail message.
     */
    public TransactionRequiredException()
    {
    }

    /**
     *  Constructs an <code>TransactionRequiredException</code> with the
     *  specified detail message.
     *
     *  @param msg the detail message.
     */
    public TransactionRequiredException(String msg)
    {
        super(msg);
    }
}
