package org.jbossmx.cluster.watchdog.util.xml;

/**
 * @author
 */
public class XMLScriptException
    extends Exception
{
    /**
     * @param    reason
     */
    public XMLScriptException(final String reason)
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