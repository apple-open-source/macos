/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public interface QueueBrowser
{
    public Queue getQueue() throws JMSException;

    public String getMessageSelector() throws JMSException;

    public java.util.Enumeration getEnumeration() throws JMSException;

    public void close() throws JMSException;

}
