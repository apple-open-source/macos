
// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: MultiException.java,v 1.15.2.5 2003/06/04 04:47:58 starksm Exp $
// ========================================================================

package org.mortbay.util;
import java.util.List;


/* ------------------------------------------------------------ */
/** Wraps multiple exceptions.
 *
 * Allows multiple exceptions to be thrown as a single exception.
 *
 * @version $Id: MultiException.java,v 1.15.2.5 2003/06/04 04:47:58 starksm Exp $
 * @author Greg Wilkins (gregw)
 */
public class MultiException extends Exception
{
    private Object nested;

    /* ------------------------------------------------------------ */
    public MultiException()
    {
        super("Multiple exceptions");
    }

    /* ------------------------------------------------------------ */
    public void add(Exception e)
    {
        if (e instanceof MultiException)
        {
            MultiException me = (MultiException)e;
            for (int i=0;i<LazyList.size(me.nested);i++)
                nested=LazyList.add(nested,LazyList.get(me.nested,i));
        }
        else
            nested=LazyList.add(nested,e);
    }

    /* ------------------------------------------------------------ */
    public int size()
    {
        return LazyList.size(nested);
    }
    
    /* ------------------------------------------------------------ */
    public List getExceptions()
    {
        return LazyList.getList(nested);
    }
    
    /* ------------------------------------------------------------ */
    public Exception getException(int i)
    {
        return (Exception) LazyList.get(nested,i);
    }

    /* ------------------------------------------------------------ */
    /** Throw a multiexception.
     * If this multi exception is empty then no action is taken. If it
     * contains a single exception that is thrown, otherwise the this
     * multi exception is thrown. 
     * @exception Exception 
     */
    public void ifExceptionThrow()
        throws Exception
    {
        switch (LazyList.size(nested))
        {
          case 0:
              break;
          case 1:
              throw (Exception)LazyList.get(nested,0);
          default:
              throw this;
        }
    }
    
    /* ------------------------------------------------------------ */
    /** Throw a multiexception.
     * If this multi exception is empty then no action is taken. If it
     * contains a any exceptions then this
     * multi exception is thrown. 
     */
    public void ifExceptionThrowMulti()
        throws MultiException
    {
        if (LazyList.size(nested)>0)
            throw this;
    }

    /* ------------------------------------------------------------ */
    public String toString()
    {
        if (LazyList.size(nested)>0)
            return "org.mortbay.util.MultiException"+
                LazyList.getList(nested);
        return "org.mortbay.util.MultiException[]";
    }

    /* ------------------------------------------------------------ */
    public void printStackTrace()
    {
        super.printStackTrace();
        for (int i=0;i<LazyList.size(nested);i++)
            ((Throwable)LazyList.get(nested,i)).printStackTrace();
    }
    
    
}
