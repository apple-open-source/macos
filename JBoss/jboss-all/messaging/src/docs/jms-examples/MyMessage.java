/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

import javax.jms.Message;
import javax.jms.JMSException;
import javax.jms.MessageFormatException;
import javax.jms.MessageNotWriteableException;
import javax.jms.Destination;
import java.util.Enumeration;
import java.util.Hashtable;
import java.io.Serializable;
import java.lang.Comparable;

/**
 *	This class implements javax.jms.Message
 *      
 *	@author Norbert Lataille (Norbert.Lataille@m4x.org)
 *	@author Hiram Chirino (Cojonudo14@hotmail.com)
 * 
 *	@version $Revision: 1.1 $
 */
public class MyMessage implements Serializable, Message {
	// Constants -----------------------------------------------------
	static final int DEFAULT_DELIVERY_MODE = -1;
	static final int DEFAULT_PRIORITY = -1;
	static final int DEFAULT_TIME_TO_LIVE = -1;
	//Those attributes are not transient ---------------
	//Header fields 
	//Set by send() method
	private Destination jmsDestination = null;
	private int jmsDeliveryMode = -1;
	private long jmsExpiration = 0;
	private int jmsPriority = -1;
	private String jmsMessageID = null;
	private long jmsTimeStamp = 0;
	//Set by the client
	private boolean jmsCorrelationID = true;
	private String jmsCorrelationIDString = null;
	private byte[] jmsCorrelationIDbyte = null;
	private Destination jmsReplyTo = null;
	private String jmsType = null;
	//Set by the provider
	private boolean jmsRedelivered = false;
	//Properties
	private Hashtable prop;
	public boolean propReadWrite;
	//Message body
	public boolean msgReadOnly = false;
	// Constructor ---------------------------------------------------
	MyMessage() {
		prop = new Hashtable();
		propReadWrite = true;
	}

	// Public --------------------------------------------------------

	public String getJMSMessageID() throws JMSException {
		return jmsMessageID;
	}

	public void setJMSMessageID(String id) throws JMSException {
		jmsMessageID = id;
	}

	public long getJMSTimestamp() throws JMSException {
		return jmsTimeStamp;
	}

	public void setJMSTimestamp(long timestamp) throws JMSException {
		jmsTimeStamp = timestamp;
	}

	public byte[] getJMSCorrelationIDAsBytes() throws JMSException {
		if (jmsCorrelationID)
			throw new JMSException("JMSCorrelationID is a string");
		return jmsCorrelationIDbyte;
	}

	public void setJMSCorrelationIDAsBytes(byte[] correlationID) throws JMSException {
		jmsCorrelationID = false;
		jmsCorrelationIDbyte = (byte[]) correlationID.clone();
		jmsCorrelationIDString = null;
	}

	public void setJMSCorrelationID(String correlationID) throws JMSException {
		jmsCorrelationID = true;
		jmsCorrelationIDString = correlationID;
		jmsCorrelationIDbyte = null;
	}

	public String getJMSCorrelationID() throws JMSException {
		if (!jmsCorrelationID)
			throw new JMSException("JMSCorrelationID is an array");
		return jmsCorrelationIDString;
	}

	public Destination getJMSReplyTo() throws JMSException {
		return jmsReplyTo;
	}

	public void setJMSReplyTo(Destination replyTo) throws JMSException {
		jmsReplyTo = replyTo;
	}

	public Destination getJMSDestination() throws JMSException {
		return jmsDestination;
	}

	public void setJMSDestination(Destination destination) throws JMSException {
		jmsDestination = destination;
	}

	public int getJMSDeliveryMode() throws JMSException {
		return jmsDeliveryMode;
	}

	public void setJMSDeliveryMode(int deliveryMode) throws JMSException {
		jmsDeliveryMode = deliveryMode;
	}

	public boolean getJMSRedelivered() throws JMSException {
		return jmsRedelivered;
	}

	public void setJMSRedelivered(boolean redelivered) throws JMSException {
		jmsRedelivered = redelivered;
	}

	public String getJMSType() throws JMSException {
		return jmsType;
	}

	public void setJMSType(String type) throws JMSException {
		jmsType = type;
	}

	public long getJMSExpiration() throws JMSException {
		return jmsExpiration;
	}

	public void setJMSExpiration(long expiration) throws JMSException {
		jmsExpiration = expiration;
	}

	public int getJMSPriority() throws JMSException {
		return jmsPriority;
	}

	public void setJMSPriority(int priority) throws JMSException {
		jmsPriority = priority;
	}

	public void clearProperties() throws JMSException {
		prop = new Hashtable();
		propReadWrite = true;
	}

	public boolean propertyExists(String name) throws JMSException {
		return prop.containsKey(name);
	}

	public boolean getBooleanProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			throw new NullPointerException();

		if (value instanceof Boolean)
			return ((Boolean) value).booleanValue();
		else if (value instanceof String)
			return Boolean.getBoolean((String) value);
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public byte getByteProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			throw new NullPointerException();

		if (value instanceof Byte)
			return ((Byte) value).byteValue();
		else if (value instanceof String)
			return Byte.parseByte((String) value);
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public short getShortProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			throw new NullPointerException();

		if (value instanceof Byte)
			return ((Byte) value).shortValue();
		else if (value instanceof Short)
			return ((Short) value).shortValue();
		else if (value instanceof String)
			return Short.parseShort((String) value);
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public int getIntProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			throw new NullPointerException();

		if (value instanceof Byte)
			return ((Byte) value).intValue();
		else if (value instanceof Short)
			return ((Short) value).intValue();
		else if (value instanceof Integer)
			return ((Integer) value).intValue();
		else if (value instanceof String)
			return Integer.parseInt((String) value);
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public long getLongProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			throw new NullPointerException();

		if (value instanceof Byte)
			return ((Byte) value).longValue();
		else if (value instanceof Short)
			return ((Short) value).longValue();
		else if (value instanceof Integer)
			return ((Integer) value).longValue();
		else if (value instanceof Long)
			return ((Long) value).longValue();
		else if (value instanceof String)
			return Long.parseLong((String) value);
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public float getFloatProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			throw new NullPointerException();

		if (value instanceof Float)
			return ((Float) value).floatValue();
		else if (value instanceof String)
			return Float.parseFloat((String) value);
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public double getDoubleProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			throw new NullPointerException();

		if (value instanceof Float)
			return ((Float) value).doubleValue();
		else if (value instanceof Double)
			return ((Double) value).doubleValue();
		else if (value instanceof String)
			return Double.parseDouble((String) value);
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public String getStringProperty(String name) throws JMSException {
		Object value = prop.get(name);
		if (value == null)
			return null;

		if (value instanceof Boolean)
			return ((Boolean) value).toString();
		else if (value instanceof Byte)
			return ((Byte) value).toString();
		else if (value instanceof Short)
			return ((Short) value).toString();
		else if (value instanceof Integer)
			return ((Integer) value).toString();
		else if (value instanceof Long)
			return ((Long) value).toString();
		else if (value instanceof Float)
			return ((Float) value).toString();
		else if (value instanceof Double)
			return ((Double) value).toString();
		else if (value instanceof String)
			return (String) value;
		else
			throw new MessageFormatException("Invalid conversion");
	}

	public Object getObjectProperty(String name) throws JMSException {
		Object value = prop.get(name);
		return value;
	}

	public Enumeration getPropertyNames() throws JMSException {
		return prop.keys();
	}

	void CheckPropertyName(String name) throws JMSException {
		if (name.regionMatches(false, 0, "JMS_", 0, 4)) {
			throw new JMSException("Bad property name");
		}

		if (name.regionMatches(false, 0, "JMSX", 0, 4)) {
			if (name.equals("JMSXGroupId"))
				return;
			if (name.equals("JMSXGroupSeq"))
				return;
			throw new JMSException("Bad property name");
		}

	}

	public void setBooleanProperty(String name, boolean value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new Boolean(value));
	}

	public void setByteProperty(String name, byte value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new Byte(value));
	}

	public void setShortProperty(String name, short value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new Short(value));
	}

	public void setIntProperty(String name, int value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new Integer(value));
	}

	public void setLongProperty(String name, long value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new Long(value));
	}

	public void setFloatProperty(String name, float value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new Float(value));
	}

	public void setDoubleProperty(String name, double value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new Double(value));
	}

	public void setStringProperty(String name, String value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");
		prop.put(name, new String(value));
	}

	public void setObjectProperty(String name, Object value) throws JMSException {
		CheckPropertyName(name);
		if (!propReadWrite)
			throw new MessageNotWriteableException("Properties are read-only");

		if (value instanceof Boolean)
			prop.put(name, value);
		else if (value instanceof Byte)
			prop.put(name, value);
		else if (value instanceof Short)
			prop.put(name, value);
		else if (value instanceof Integer)
			prop.put(name, value);
		else if (value instanceof Long)
			prop.put(name, value);
		else if (value instanceof Float)
			prop.put(name, value);
		else if (value instanceof Double)
			prop.put(name, value);
		else if (value instanceof String)
			prop.put(name, value);
		else
			throw new MessageFormatException("Invalid object type");
	}

	public void clearBody() throws JMSException {
		//Inherited classes clear their content here
		msgReadOnly = false;
	}

	/**
	 * acknowledge method comment.
	 */
	public void acknowledge() throws javax.jms.JMSException {
	}

	void setReadOnlyMode() {
		propReadWrite = false;
		msgReadOnly = true;
	}

	public boolean isOutdated() {
		if (jmsExpiration == 0)
			return false;
		long ts = System.currentTimeMillis();
		return jmsExpiration < ts;
	}

	String text;

	/**
	 * Compares this object with the specified object for order.  Returns a
	 * negative integer, zero, or a positive integer as this object is less
	 * than, equal to, or greater than the specified object.<p>
	 *
	 * The implementor must ensure <tt>sgn(x.compareTo(y)) ==
	 * -sgn(y.compareTo(x))</tt> for all <tt>x</tt> and <tt>y</tt>.  (This
	 * implies that <tt>x.compareTo(y)</tt> must throw an exception iff
	 * <tt>y.compareTo(x)</tt> throws an exception.)<p>
	 *
	 * The implementor must also ensure that the relation is transitive:
	 * <tt>(x.compareTo(y)&gt;0 &amp;&amp; y.compareTo(z)&gt;0)</tt> implies
	 * <tt>x.compareTo(z)&gt;0</tt>.<p>
	 *
	 * Finally, the implementer must ensure that <tt>x.compareTo(y)==0</tt>
	 * implies that <tt>sgn(x.compareTo(z)) == sgn(y.compareTo(z))</tt>, for
	 * all <tt>z</tt>.<p>
	 *
	 * It is strongly recommended, but <i>not</i> strictly required that
	 * <tt>(x.compareTo(y)==0) == (x.equals(y))</tt>.  Generally speaking, any
	 * class that implements the <tt>Comparable</tt> interface and violates
	 * this condition should clearly indicate this fact.  The recommended
	 * language is "Note: this class has a natural ordering that is
	 * inconsistent with equals."
	 * 
	 * @param   o the Object to be compared.
	 * @return  a negative integer, zero, or a positive integer as this object
	 *		is less than, equal to, or greater than the specified object.
	 * 
	 * @throws ClassCastException if the specified object's type prevents it
	 *         from being compared to this Object.
	 */
	public int compareTo(java.lang.Object o) {
		return 0;
	}

	/**
	 * Insert the method's description here.
	 * Creation date: (12/31/00 5:32:58 PM)
	 * @return java.lang.String
	 */
	public java.lang.String getText() {
		return text;
	}

	/**
	 * Insert the method's description here.
	 * Creation date: (12/31/00 5:32:58 PM)
	 * @param newText java.lang.String
	 */
	public void setText(java.lang.String newText) {
		text = newText;
	}

}
