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
  * @version $Revision: 1.1.8.1 $
 **/
public interface Message
{
    // CONSTANTS ----------------------------------------------------

    public static final int DEFAULT_DELIVERY_MODE = DeliveryMode.PERSISTENT;
    public static final int DEFAULT_PRIORITY = 4;
    /** FIXME - is this correct - default should be unlimited ??? */
    public static final long DEFAULT_TIME_TO_LIVE = 0;

    // METHODS -------------------------------------------------------

    public String getJMSMessageID() throws JMSException;

    public void setJMSMessageID(String messageID) throws JMSException;

    public long getJMSTimestamp() throws JMSException;

    public void setJMSTimestamp(long timestamp) throws JMSException;

    public byte[] getJMSCorrelationIDAsBytes() throws JMSException;

    public void setJMSCorrelationIDAsBytes(byte[] correlationID) throws JMSException;

    public void setJMSCorrelationID(String correlationID) throws JMSException;

    public String getJMSCorrelationID() throws JMSException;

    public Destination getJMSReplyTo() throws JMSException;

    public void setJMSReplyTo(Destination replyTo) throws JMSException;

    public Destination getJMSDestination() throws JMSException;

    public void setJMSDestination(Destination destination) throws JMSException;

    public int getJMSDeliveryMode() throws JMSException;

    public void setJMSDeliveryMode(int deliveryMode) throws JMSException;

    public boolean getJMSRedelivered() throws JMSException;

    public void setJMSRedelivered(boolean redelivered) throws JMSException;

    public String getJMSType() throws JMSException;

    public void setJMSType(String type) throws JMSException;

    public long getJMSExpiration() throws JMSException;

    public void setJMSExpiration(long expiration) throws JMSException;

    public int getJMSPriority() throws JMSException;

    public void setJMSPriority(int priority) throws JMSException;


    public void clearProperties() throws JMSException;
    public boolean propertyExists(String name) throws JMSException;


    public boolean getBooleanProperty(String name) throws JMSException;
    public byte getByteProperty(String name) throws JMSException;
    public short getShortProperty(String name) throws JMSException;
    public int getIntProperty(String name) throws JMSException;
    public long getLongProperty(String name) throws JMSException;
    public float getFloatProperty(String name) throws JMSException;
    public double getDoubleProperty(String name) throws JMSException;
    public String getStringProperty(String name) throws JMSException;
    public Object getObjectProperty(String name) throws JMSException;

    public java.util.Enumeration getPropertyNames() throws JMSException;

    public void setBooleanProperty(String name, boolean value) throws JMSException;
    public void setByteProperty(String name, byte value) throws JMSException;
    public void setShortProperty(String name, short value) throws JMSException;
    public void setIntProperty(String name, int value) throws JMSException;
    public void setLongProperty(String name, long value) throws JMSException;
    public void setFloatProperty(String name, float value) throws JMSException;
    public void setDoubleProperty(String name, double value) throws JMSException;
    public void setStringProperty(String name, String value) throws JMSException;
    public void setObjectProperty(String name, Object value) throws JMSException;

    public void acknowledge() throws JMSException;

    public void clearBody() throws JMSException;
}
