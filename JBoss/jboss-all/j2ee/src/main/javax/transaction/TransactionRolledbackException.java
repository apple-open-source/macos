/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

import java.rmi.RemoteException;

/**
 *  This exception indicates that the transaction context carried in the
 *  remote invocation has been rolled back, or was marked for roll back only.
 *
 *  This indicates to the client that further computation on behalf of this
 *  transaction would be fruitless.
 *
 * @version $Revision: 1.2 $
 */
public class TransactionRolledbackException extends RemoteException
{

    /**
     *  Creates a new <code>TransactionRolledbackException</code> without
     *  a detail message.
     */
    public TransactionRolledbackException()
    {
    }

    /**
     *  Constructs an <code>TransactionRolledbackException</code> with the
     *  specified detail message.
     *
     *  @param msg the detail message.
     */
    public TransactionRolledbackException(String msg)
    {
        super(msg);
    }
}
