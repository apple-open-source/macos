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
public interface QueueSession extends Session
{
    public Queue createQueue(String queueName) throws JMSException;

    public QueueReceiver createReceiver(Queue queue) throws JMSException;

    public QueueReceiver createReceiver(Queue queue,
                                        String messageSelector) throws JMSException;

    public QueueSender createSender(Queue queue) throws JMSException;

    public QueueBrowser createBrowser(Queue queue) throws JMSException;

    public QueueBrowser createBrowser(Queue queue,
                                      String messageSelector) throws JMSException;

    public TemporaryQueue createTemporaryQueue() throws JMSException;
}
