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
public interface TopicPublisher extends MessageProducer
{
    public Topic getTopic() throws JMSException;

    public void publish(Message message) throws JMSException;

    public void publish(Message message,
                        int deliveryMode,
                        int priority,
                        long timeToLive) throws JMSException;

    public void publish(Topic topic,
                        Message message) throws JMSException;

    public void publish(Topic topic,
                        Message message,
                        int deliveryMode,
                        int priority,
                        long timeToLive) throws JMSException;


}
