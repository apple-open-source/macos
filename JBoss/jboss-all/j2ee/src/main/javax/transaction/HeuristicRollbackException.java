/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  This exception is thrown by the commit operation to report that a
 *  heuristic decision was made and that all relevant updates have been
 *  rolled back. 
 *
 *  @version $Revision: 1.2 $
 */
public class HeuristicRollbackException extends Exception
{

    /**
     *  Creates a new <code>HeuristicRollbackException</code> without a
     *  detail message.
     */
    public HeuristicRollbackException()
    {
    }

    /**
     *  Constructs an <code>HeuristicRollbackException</code> with the
     *  specified detail message.
     *
     *  @param msg the detail message.
     */
    public HeuristicRollbackException(String msg)
    {
        super(msg);
    }
}
