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
public interface QueueSender extends MessageProducer
{
    public Queue getQueue() throws JMSException;

    public void send(Message message) throws JMSException;

    public void send(Message message,
                     int deliveryMode,
                     int priority,
                     long timeToLive) throws JMSException;

    public void send(Queue queue,
                     Message message) throws JMSException;

    public void send(Queue queue,
                     Message message,
                     int deliveryMode,
                     int priority,
                     long timeToLive) throws JMSException;


}
