/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * Provides a basic request/reply layer ontop of JMS.
  * Pass the constructor details of the session/topic to send requests upon.
  * Then call the request method to send a request.  The method will block
  * until the reply is received.
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1.8.1 $
 **/
public class TopicRequestor
{
    // CONSTRUCTOR -----------------------------------------


    public TopicRequestor(TopicSession session,
                          Topic topic) throws JMSException
    {
        _topicSession = session;
        _topic = topic;
        _requestPublisher   = _topicSession.createPublisher(_topic);
        _responseTopic      = _topicSession.createTemporaryTopic();
        _responseSubscriber = _topicSession.createSubscriber(_responseTopic);
    }

    // METHODS -------------------------------------------------

    public Message request(Message message) throws JMSException
    {
        message.setJMSReplyTo(_responseTopic);
        _requestPublisher.publish(message);
        return _responseSubscriber.receive();
    }

    public void close() throws JMSException
    {
        _responseTopic.delete();
        _topicSession.close();
    }


    // INSTANCE VARIABLES -------------------------------------

    private TopicSession _topicSession = null;
    private Topic _topic = null;
    private TopicPublisher _requestPublisher = null;
    private TemporaryTopic _responseTopic = null;
    private TopicSubscriber _responseSubscriber = null;
}
