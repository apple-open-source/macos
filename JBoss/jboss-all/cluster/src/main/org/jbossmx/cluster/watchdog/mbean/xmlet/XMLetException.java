/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.mbean.xmlet;

/**
 * @author
 */
public class XMLetException
    extends Exception
{
    /**
     * @param    reason
     */
    public XMLetException(final String reason)
    {
        m_reason = reason;
    }

    /**
     *
     * @return
     */
    public String toString()
    {
        return m_reason;
    }

    /** */
    private String m_reason;
}
