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
public interface TopicSession extends Session
{
    public Topic createTopic(String topicName) throws JMSException;

    public TopicSubscriber createSubscriber(Topic topic) throws JMSException;

    public TopicSubscriber createSubscriber(Topic topic,
                                            String messageSelector,
                                            boolean noLocal) throws JMSException;

    public TopicSubscriber createDurableSubscriber(Topic topic,
                                                   String name) throws JMSException;

    public TopicSubscriber createDurableSubscriber(Topic topic,
                                                   String name,
                                                   String messageSelector,
                                                   boolean noLocal) throws JMSException;

    public TopicPublisher createPublisher(Topic topic) throws JMSException;

    public TemporaryTopic createTemporaryTopic() throws JMSException;

    public void unsubscribe(String name) throws JMSException;
}
