/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

// Standard Java Packages

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

/**
 *
 */
public class CompoundException
    extends Exception
{
    /**
     */
    public CompoundException()
    {
        m_exceptions = new LinkedList();
        m_toStringBuffer = new StringBuffer();
    }

    /**
     * @param    exception
     */
    public void addException(Exception exception)
    {
        if(exception != null)
        {
            exception.printStackTrace();
            m_exceptions.add(exception);
            m_toStringBuffer.append(exception.toString());
        }
    }

    /**
     */
    public void printStackTrace()
    {
        System.err.println("CompoundException:\n{");

        for (Iterator i = m_exceptions.iterator(); i.hasNext();)
        {
            ((Exception) i.next()).printStackTrace(System.err);
        }

        //System.err.println(toString());
        System.err.println("}\nCompoundException");
    }

    /**
     *
     * @return
     */
    public String toString()
    {
        return m_toStringBuffer.toString();
    }

    /** */
    private List m_exceptions;
    /** */
    private StringBuffer m_toStringBuffer;
}
