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
public interface MessageConsumer
{
    public String getMessageSelector() throws JMSException;

    public MessageListener getMessageListener() throws JMSException;

    public void setMessageListener(MessageListener listener) throws JMSException;

    public Message receive() throws JMSException;

    public Message receive(long timeout) throws JMSException;

    public Message receiveNoWait() throws JMSException;

    public void close() throws JMSException;

}
