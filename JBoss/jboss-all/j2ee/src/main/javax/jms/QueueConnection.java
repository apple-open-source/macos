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
public interface QueueConnection extends Connection
{
    public QueueSession createQueueSession(boolean transacted,
                                           int acknowledgeMode)
    throws JMSException;

    public ConnectionConsumer createConnectionConsumer(Queue queue, 
                                                       String messageSelector,
                                                       ServerSessionPool sessionPool,
                                                       int maxMessages) throws JMSException;
}
