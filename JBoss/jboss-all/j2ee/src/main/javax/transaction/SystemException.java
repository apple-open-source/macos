/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  This exception is thrown to indicate that the transaction manager has
 *  encountered an unexpected error condition that prevents future
 *  transaction services from proceeding. 
 *
 *  @version $Revision: 1.3 $
 */
public class SystemException extends Exception
{
    /**
     *  The error code of this exception. Values of this field are not
     *  specified by JTA.
     */
    public int errorCode;

    /**
     *  Creates a new <code>SystemException</code> without a detail message.
     */
    public SystemException()
    {
    }

    /**
     *  Constructs an <code>SystemException</code> with the specified
     *  detail message.
     *
     *  @param msg the detail message.
     */
    public SystemException(String msg)
    {
        super(msg);
    }

    /**
     *  Constructs an <code>SystemException</code> with the specified
     *  detail message.
     *
     *  @param errcode the error code for the exception
     */
    public SystemException(int errcode)
    {
        this.errorCode = errcode;
    }
}
