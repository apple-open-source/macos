/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.transaction;

/**
 *  This exception is meant to be thrown by the rollback operation on
 *  a resource to report that a heuristic decision was made and that all
 *  relevant updates have been committed. 
 *  <p>
 *  But though defined in JTA this exception is used nowhere in JTA, and
 *  it seems impossible to report a heuristic commit decision with the JTA
 *  API in a portable way.
 *
 *  @version $Revision: 1.2 $
 */
public class HeuristicCommitException extends Exception
{

    /**
     *  Creates a new <code>HeuristicMixedException</code> without a
     *  detail message.
     */
    public HeuristicCommitException()
    {
    }

    /**
     *  Constructs an <code>HeuristicCommitException</code> with the
     *  specified detail message.
     *
     *  @param msg the detail message.
     */
    public HeuristicCommitException(String msg)
    {
        super(msg);
    }
}
