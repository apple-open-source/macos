/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog;

/**
 * Exception class to use for progating exception outside of Hermes
 *
 * @author Stacy Curl
 */
public class HermesException
    extends Exception
{
    /**
     * Constructor for HermesException
     *
     * @param    reason the reason for the exception
     * @param    source
     */
    public HermesException(String reason, Exception source)
    {
        m_reason = reason;
        m_source = source;
    }

    public String getMessage()
    {
        return ( this.toString() );
    }

    public String toString() {

        StringBuffer buffer = new StringBuffer();

        buffer.append( m_reason );

        if ( m_source != null ) {
            buffer.append( " because " );
            buffer.append( m_source.getMessage() );
        }

        return buffer.toString();
    }

    protected String m_reason;
    protected Exception m_source;
}
