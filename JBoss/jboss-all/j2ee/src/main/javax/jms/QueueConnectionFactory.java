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
public interface QueueConnectionFactory extends ConnectionFactory
{
    public QueueConnection createQueueConnection() throws JMSException;

    public QueueConnection createQueueConnection(String username,
                                                 String password) throws JMSException;


}
