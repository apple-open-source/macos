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
public interface MessageProducer
{
    public void setDisableMessageID(boolean value) throws JMSException;

    public boolean getDisableMessageID() throws JMSException;

    public void setDisableMessageTimestamp(boolean value) throws JMSException;

    public boolean getDisableMessageTimestamp() throws JMSException;

    public void setDeliveryMode(int deliveryMode) throws JMSException;

    public int getDeliveryMode() throws JMSException;

    public void setPriority(int defaultPriorityFromLow0High9) throws JMSException;

    public int getPriority() throws JMSException;

    public void setTimeToLive(long timeToLiveInMilliseconds) throws JMSException;

    public long getTimeToLive() throws JMSException;

    public void close() throws JMSException;
}
