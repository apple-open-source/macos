/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  This exception is thrown to report that a heuristic decision was made and
 *  that some some parts of the transaction have been committed while other
 *  parts have been rolled back. 
 *
 *  @version $Revision: 1.2 $
 */
public class HeuristicMixedException extends Exception
{

    /**
     *  Creates a new <code>HeuristicMixedException</code> without a
     *  detail message.
     */
    public HeuristicMixedException()
    {
    }

    /**
     *  Constructs an <code>HeuristicMixedException</code> with the
     *  specified detail message.
     *
     *  @param msg the detail message.
     */
    public HeuristicMixedException(String msg)
    {
        super(msg);
    }
}
