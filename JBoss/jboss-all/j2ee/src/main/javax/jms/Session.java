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
  * @version $Revision: 1.2 $
 **/
public interface Session extends Runnable
{
    public static final int AUTO_ACKNOWLEDGE    = 1;
    public static final int CLIENT_ACKNOWLEDGE  = 2;
    public static final int DUPS_OK_ACKNOWLEDGE = 3;

    public BytesMessage createBytesMessage() throws JMSException;

    public MapMessage createMapMessage() throws JMSException;

    public Message createMessage() throws JMSException;

    public ObjectMessage createObjectMessage() throws JMSException;

    public ObjectMessage createObjectMessage(java.io.Serializable object) throws JMSException;

    public StreamMessage createStreamMessage() throws JMSException;

    public TextMessage createTextMessage() throws JMSException;

    public TextMessage createTextMessage(String text) throws JMSException;

    public boolean getTransacted() throws JMSException;

    public void commit() throws JMSException;

    public void rollback() throws JMSException;

    public void close() throws JMSException;

    public void recover() throws JMSException;

    public MessageListener getMessageListener() throws JMSException;

    public void setMessageListener(MessageListener listener) throws JMSException;

}
