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
  * @version $Revision: 1.2.8.1 $
 **/
public class QueueRequestor
{
    // CONSTRUCTOR ------------------------------------------

    public QueueRequestor(QueueSession session,
                          Queue queue) throws JMSException
    {
        _queueSession = session;
        _queue = queue;

        _requestSender = _queueSession.createSender(_queue);
        _replyQueue = _queueSession.createTemporaryQueue();
        _replyReceiver = _queueSession.createReceiver(_replyQueue);
    }

    // PUBLIC METHODS ------------------------------------------

    public Message request(Message message) throws JMSException
    {
        message.setJMSReplyTo(_replyQueue);
        message.setJMSDeliveryMode(DeliveryMode.NON_PERSISTENT);
        _requestSender.send(message);
        return _replyReceiver.receive();
    }

    public void close() throws JMSException
    {
        _replyQueue.delete();
        _queueSession.close();
    }

    // INSTANCE VARIABLES ----------------------------------------

    private QueueSession _queueSession = null;
    private Queue _queue = null;
    private QueueSender _requestSender = null;
    private QueueReceiver _replyReceiver = null;
    private TemporaryQueue _replyQueue = null;

}
