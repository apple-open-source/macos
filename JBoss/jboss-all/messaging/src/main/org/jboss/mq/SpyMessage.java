/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import javax.jms.Message;
import javax.jms.JMSException;
import javax.jms.IllegalStateException;
import javax.jms.MessageFormatException;
import javax.jms.MessageNotWriteableException;
import javax.jms.Destination;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;

import java.io.Externalizable;
import java.io.Serializable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;

import java.lang.Comparable;

import org.jboss.util.Strings;

/**
 * This class implements javax.jms.Message
 *
 * @author Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author Hiram Chirino (Cojonudo14@hotmail.com)
 * @author David Maplesden (David.Maplesden@orion.co.nz)
 * @version $Revision: 1.19.2.4 $
 */
public class SpyMessage
   implements Serializable, Message, Comparable, Externalizable
{
   // So that every time we rebuild the sources we don't have
   // Version serialization problems
   private final static long serialVersionUID = 467206190892964404L;

   // Constants -----------------------------------------------------
   public static final int DEFAULT_DELIVERY_MODE = javax.jms.DeliveryMode.PERSISTENT;
   public static final int DEFAULT_PRIORITY = 4;
   public static final int DEFAULT_TIME_TO_LIVE = 0;

   /**
    * JBoss-vendor specific property for scheduling a JMS message.
    * In milliseconds since January 1, 1970.
    */
   public static final String PROPERTY_SCHEDULED_DELIVERY = "JMS_JBOSS_SCHEDULED_DELIVERY";

   /**
    * JBoss-vendor specific property specifying redelivery delay of a message.
    * The message will be rescheduled for delivery from the time at which it
    * was unacknowledged, plus the given period.
    */
   public static final String PROPERTY_REDELIVERY_DELAY = "JMS_JBOSS_REDELIVERY_DELAY";

   /**
    * JBoss-vendor specific property for getting the count of redelivery attempts
    * of a message.
    */
   public static final String PROPERTY_REDELIVERY_COUNT = "JMS_JBOSS_REDELIVERY_COUNT";

   /**
    * JBoss-vendor specific property specifying the limit of redelivery attempts
    * of a message.
    * The message will be redelivered a given number of times.
    * If not set, the container default is used.
    */
   public static final String PROPERTY_REDELIVERY_LIMIT = "JMS_JBOSS_REDELIVERY_LIMIT";

   // Reserved identifiers ----------------------------

   private static final HashSet reservedIdentifiers = new HashSet();

   static
   {
      reservedIdentifiers.add("NULL");
      reservedIdentifiers.add("TRUE");
      reservedIdentifiers.add("FALSE");
      reservedIdentifiers.add("NOT");
      reservedIdentifiers.add("AND");
      reservedIdentifiers.add("OR");
      reservedIdentifiers.add("BETWEEN");
      reservedIdentifiers.add("LIKE");
      reservedIdentifiers.add("IN");
      reservedIdentifiers.add("IS");
      reservedIdentifiers.add("ESCAPE");
   }

   //Those attributes are not transient ---------------
   //Header fields
   public static class Header {
      //Set by send() method
      public Destination jmsDestination = null;
      public int jmsDeliveryMode = -1;
      public long jmsExpiration = 0;
      public int jmsPriority = -1;
      public String jmsMessageID = null;
      public long jmsTimeStamp = 0;
      //Set by the client
      public boolean jmsCorrelationID = true;
      public String jmsCorrelationIDString = null;
      public byte[] jmsCorrelationIDbyte = null;
      public Destination jmsReplyTo = null;
      public String jmsType = null;
      //Set by the provider
      public boolean jmsRedelivered = false;
      //Properties
      public HashMap jmsProperties = new HashMap();
      public boolean jmsPropertiesReadWrite=true;
      //Message body
      public boolean msgReadOnly = false;
      //For noLocal to be able to tell if this was a locally produced message
      public String producerClientId;
      //For durable subscriptions
      public DurableSubscriptionID durableSubscriberID = null;
      //For ordering in the JMSServerQueue (set on the server side)
      public transient long messageId;
      
      public String toString() {
         return "Header { \n"+
                "   jmsDestination  : "+jmsDestination+"\n"+
                "   jmsDeliveryMode : "+jmsDeliveryMode+"\n"+
                "   jmsExpiration   : "+jmsExpiration+"\n"+
                "   jmsPriority     : "+jmsPriority+"\n"+
                "   jmsMessageID    : "+jmsMessageID+"\n"+
                "   jmsTimeStamp    : "+jmsTimeStamp+"\n"+
                "   jmsCorrelationID: "+jmsCorrelationIDString+"\n"+
                "   jmsReplyTo      : "+jmsReplyTo+"\n"+
                "   jmsType         : "+jmsType+"\n"+
                "   jmsRedelivered  : "+jmsRedelivered+"\n"+
                "   jmsProperties   : "+jmsProperties+"\n"+
                "   jmsPropertiesReadWrite:"+jmsPropertiesReadWrite+"\n"+
                "   msgReadOnly     : "+msgReadOnly+"\n"+
                "   producerClientId: "+producerClientId+"\n"+
         	      "}";
      }
   }

   public Header header = new Header();
   public transient AcknowledgementRequest ack;

   // Transient Attributes ------------------------------------------
   //For acknowledgment (set on the client side)
   public transient SpySession session;

   // Constructor ---------------------------------------------------
   public SpyMessage() {
   }

   // Public --------------------------------------------------------

   public String getJMSMessageID() {
      return header.jmsMessageID;
   }

   public void setJMSMessageID(String id) throws JMSException {
      header.jmsMessageID = id;
   }

   public long getJMSTimestamp() {
      return header.jmsTimeStamp;
   }

   public void setJMSTimestamp(long timestamp) throws JMSException {
      header.jmsTimeStamp = timestamp;
   }

   public byte[] getJMSCorrelationIDAsBytes() throws JMSException {
      if (header.jmsCorrelationID)
         throw new JMSException("JMSCorrelationID is a string");
      return header.jmsCorrelationIDbyte;
   }

   public void setJMSCorrelationIDAsBytes(byte[] correlationID) throws JMSException {
      header.jmsCorrelationID = false;
      header.jmsCorrelationIDbyte = (byte[]) correlationID.clone();
      header.jmsCorrelationIDString = null;
   }

   public void setJMSCorrelationID(String correlationID) throws JMSException {
      header.jmsCorrelationID = true;
      header.jmsCorrelationIDString = correlationID;
      header.jmsCorrelationIDbyte = null;
   }

   public String getJMSCorrelationID() throws JMSException {
      if (!header.jmsCorrelationID)
         throw new JMSException("JMSCorrelationID is an array");
      return header.jmsCorrelationIDString;
   }

   public Destination getJMSReplyTo() {
      return header.jmsReplyTo;
   }

   public void setJMSReplyTo(Destination replyTo) throws JMSException {
      header.jmsReplyTo = replyTo;
   }

   public Destination getJMSDestination() {
      return header.jmsDestination;
   }

   public void setJMSDestination(Destination destination) throws JMSException {
      header.jmsDestination = destination;
   }

   public int getJMSDeliveryMode() {
      return header.jmsDeliveryMode;
   }

   public void setJMSDeliveryMode(int deliveryMode) throws JMSException {
      header.jmsDeliveryMode = deliveryMode;
   }

   public boolean getJMSRedelivered() {
      return header.jmsRedelivered;
   }

   public void setJMSRedelivered(boolean redelivered) throws JMSException {
      header.jmsRedelivered = redelivered;
   }

   public String getJMSType() {
      return header.jmsType;
   }

   public void setJMSType(String type) throws JMSException {
      header.jmsType = type;
   }

   public long getJMSExpiration() {
      return header.jmsExpiration;
   }

   public void setJMSExpiration(long expiration) throws JMSException {
      header.jmsExpiration = expiration;
   }

   public int getJMSPriority() {
      return header.jmsPriority;
   }

   public void setJMSPriority(int priority) throws JMSException {
      if (priority < 0 || priority > 10)
         throw new JMSException("Unsupported priority '"+priority+"': priority must be from 0-10");
      header.jmsPriority = priority;
   }

   public void clearProperties() throws JMSException {
      header.jmsProperties.clear();
      header.jmsPropertiesReadWrite = true;
   }

   public boolean propertyExists(String name) throws JMSException {
      return header.jmsProperties.containsKey(name);
   }

   public boolean getBooleanProperty(String name) throws JMSException {
      Object value = header.jmsProperties.get(name);
      if (value == null)
         return Boolean.valueOf(null).booleanValue();

      if (value instanceof Boolean)
         return ((Boolean) value).booleanValue();
      else if (value instanceof String)
         return Boolean.valueOf((String) value).booleanValue();
      else
         throw new MessageFormatException("Invalid conversion");
   }

   public byte getByteProperty(String name) throws JMSException {
      Object value = header.jmsProperties.get(name);
      if (value == null)
         throw new NumberFormatException("Message property '"+name+"' not set.");

      if (value instanceof Byte)
         return ((Byte) value).byteValue();
      else if (value instanceof String)
         return Byte.parseByte((String) value);
      else
         throw new MessageFormatException("Invalid conversion");
   }

   public short getShortProperty(String name) throws JMSException {
      Object value = header.jmsProperties.get(name);
      if (value == null)
         throw new NumberFormatException("Message property '"+name+"' not set.");

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
      Object value = header.jmsProperties.get(name);
      if (value == null)
         throw new NumberFormatException("Message property '"+name+"' not set.");

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
      Object value = header.jmsProperties.get(name);
      if (value == null)
         throw new NumberFormatException("Message property '"+name+"' not set.");

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
      Object value = header.jmsProperties.get(name);
      if (value == null)
         return Float.valueOf(null).floatValue();

      if (value instanceof Float)
         return ((Float) value).floatValue();
      else if (value instanceof String)
         return Float.parseFloat((String) value);
      else
         throw new MessageFormatException("Invalid conversion");
   }

   public double getDoubleProperty(String name) throws JMSException {
      Object value = header.jmsProperties.get(name);
      if (value == null)
         return Double.valueOf(null).doubleValue();

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
      Object value = header.jmsProperties.get(name);
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
      Object value = header.jmsProperties.get(name);
      return value;
   }

   public Enumeration getPropertyNames() throws JMSException {
      Enumeration names = Collections.enumeration(header.jmsProperties.keySet());
      return names;
   }

   void checkProperty(String name, Object value) throws JMSException {
      if( name == null )
         throw new IllegalArgumentException( "The name of a property must not be null." );

      if( name.equals("") )
         throw new IllegalArgumentException( "The name of a property must not be an empty String." );

      if (Strings.isValidJavaIdentifier(name) == false)
         throw new IllegalArgumentException("The property name '" + name + "' is not a valid java identifier.");

      if (reservedIdentifiers.contains(name))
         throw new IllegalArgumentException("The property name '" + name + "' is reserved due to selector syntax.");

      if (name.regionMatches(false, 0, "JMS_", 0, 4))
      {
         if (name.equals(PROPERTY_SCHEDULED_DELIVERY))
         {
            if (!(value instanceof Long))
               throw new JMSException(name + " must be Long: " + value);
         }
         else if (name.equals(PROPERTY_REDELIVERY_DELAY))
         {
            if (!(value instanceof Number))
               throw new JMSException(name + " must be Number: " + value);
         }
         else if (name.equals(PROPERTY_REDELIVERY_COUNT))
         {
            if (!(value instanceof Number))
               throw new JMSException(name + " must be Number: " + value);
         }
         else if (name.equals(PROPERTY_REDELIVERY_LIMIT))
         {
            if (!(value instanceof Number))
               throw new JMSException(name + " must be Number: " + value);
         }
         else
         {
            throw new JMSException("Illegal property name: " + name);
         }
      }

      if (name.regionMatches(false, 0, "JMSX", 0, 4)) {
         if (name.equals("JMSXGroupID"))
            return;
         if (name.equals("JMSXGroupSeq"))
            return;
         throw new JMSException("Illegal property name: " + name);
      }

   }

   public void setBooleanProperty(String name, boolean value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      Boolean b = new Boolean(value);
      checkProperty(name, b);
      header.jmsProperties.put(name, b);
   }

   public void setByteProperty(String name, byte value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      Byte b = new Byte(value);
      checkProperty(name, b);
      header.jmsProperties.put(name, b);
   }

   public void setShortProperty(String name, short value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      Short s = new Short(value);
      checkProperty(name, s);
      header.jmsProperties.put(name, s);
   }

   public void setIntProperty(String name, int value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      Integer i = new Integer(value);
      checkProperty(name, i);
      header.jmsProperties.put(name, i);
   }

   public void setLongProperty(String name, long value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      Long l = new Long(value);
      checkProperty(name, l);
      header.jmsProperties.put(name, l);
   }

   public void setFloatProperty(String name, float value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      Float f = new Float(value);
      checkProperty(name, f);
      header.jmsProperties.put(name, f);
   }

   public void setDoubleProperty(String name, double value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      Double d = new Double(value);
      checkProperty(name, d);
      header.jmsProperties.put(name, d);
   }

   public void setStringProperty(String name, String value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      checkProperty(name, value);
      header.jmsProperties.put(name, value);
   }

   public void setObjectProperty(String name, Object value) throws JMSException {
      if (!header.jmsPropertiesReadWrite)
         throw new MessageNotWriteableException("Properties are read-only");
      checkProperty(name, value);
      if (value instanceof Boolean)
         header.jmsProperties.put(name, value);
      else if (value instanceof Byte)
         header.jmsProperties.put(name, value);
      else if (value instanceof Short)
         header.jmsProperties.put(name, value);
      else if (value instanceof Integer)
         header.jmsProperties.put(name, value);
      else if (value instanceof Long)
         header.jmsProperties.put(name, value);
      else if (value instanceof Float)
         header.jmsProperties.put(name, value);
      else if (value instanceof Double)
         header.jmsProperties.put(name, value);
      else if (value instanceof String)
         header.jmsProperties.put(name, value);
      else if (value == null)
         header.jmsProperties.put(name, null);
      else
         throw new MessageFormatException("Invalid object type");
   }

   public void clearBody() throws JMSException {
      //Inherited classes clear their content here
      header.msgReadOnly = false;
   }

   public void acknowledge() throws JMSException {
      if (session == null)
         throw new JMSException("This message was not recieved from the provider");

      if (session.acknowledgeMode == session.CLIENT_ACKNOWLEDGE)
         doAcknowledge();

   }

   public void setReadOnlyMode() {
      header.jmsPropertiesReadWrite = false;
      header.msgReadOnly = true;
   }

   public SpyMessage myClone() throws JMSException {
      SpyMessage result = MessagePool.getMessage();
      result.copyProps(this);
      return result;
   }

   public void copyProps(SpyMessage original) throws JMSException {
      try {
         this.setJMSCorrelationID(original.getJMSCorrelationID());
      } catch (JMSException e) {
         //must be as bytes
         this.setJMSCorrelationIDAsBytes(original.getJMSCorrelationIDAsBytes());
      }
      this.setJMSDeliveryMode(original.getJMSDeliveryMode());
      this.setJMSDestination(original.getJMSDestination());
      this.setJMSExpiration(original.getJMSExpiration());
      this.setJMSMessageID(original.getJMSMessageID());
      this.setJMSPriority(original.getJMSPriority());
      this.setJMSRedelivered(original.getJMSRedelivered());
      this.setJMSReplyTo(original.getJMSReplyTo());
      this.setJMSTimestamp(original.getJMSTimestamp());
      this.setJMSType(original.getJMSType());
      for (Enumeration en = original.getPropertyNames(); en.hasMoreElements();) {
         String key = (String) en.nextElement();
         header.jmsProperties.put(key, original.header.jmsProperties.get(key));
      }

      //Spy Message special header.jmsPropertiess
      this.header.jmsPropertiesReadWrite = original.header.jmsPropertiesReadWrite;
      this.header.msgReadOnly = original.header.msgReadOnly;
      this.header.producerClientId = original.header.producerClientId;
      if (original.header.durableSubscriberID != null)
         this.header.durableSubscriberID = new DurableSubscriptionID(
         		original.header.durableSubscriberID.clientID, 
         		original.header.durableSubscriberID.subscriptionName,
         		original.header.durableSubscriberID.selector
         		);
   }

   // Is this method needed?
   // BasicQueue is supposed to expire this for us
   public boolean isOutdated() {
      if (header.jmsExpiration == 0)
         return false;
      long ts = System.currentTimeMillis();
      return header.jmsExpiration < ts;
   }

   /**
    * Return a negative number if this message should be sent
    * before the o message. Return a positive if should be sent
    * after the o message.
    */
   public int compareTo(Object o) {
      SpyMessage sm = (SpyMessage) o;

      if (header.jmsPriority > sm.header.jmsPriority) {
         return -1;
      }
      if (header.jmsPriority < sm.header.jmsPriority) {
         return 1;
      }
      return (int) (header.messageId - sm.header.messageId);
   }

   public void doAcknowledge() throws JMSException {
      if (session.closed) {
         throw new IllegalStateException("Session is closed.");
      }
      session.doAcknowledge( this, getAcknowledgementRequest( true ) );
   }

   public void doNegAcknowledge() throws JMSException {
      if (session.closed) {
         throw new IllegalStateException("Session is closed.");
      }
      session.doAcknowledge( this, getAcknowledgementRequest( false ) );
   }

   public void createAcknowledgementRequest(int subscriptionId) {
      ack = new AcknowledgementRequest();
      ack.destination = header.jmsDestination;
      ack.messageID = header.jmsMessageID;
      ack.subscriberId = subscriptionId;
   }

   public AcknowledgementRequest getAcknowledgementRequest(boolean isAck) throws JMSException {
      //don't know if we have to copy but to be on safe side...
      AcknowledgementRequest item = new AcknowledgementRequest();
      item.destination = ack.destination;
      item.messageID = ack.messageID;
      item.subscriberId = ack.subscriberId;
      item.isAck = isAck;
      return item;
   }

   //Custom serialization to attempt to get (big) speed improvement on message passing and persistence
   protected static final byte OBJECT_MESS = 1;
   protected static final byte BYTES_MESS = 2;
   protected static final byte MAP_MESS = 3;
   protected static final byte TEXT_MESS = 4;
   protected static final byte STREAM_MESS = 5;
   protected static final byte ENCAP_MESS = 6;
   protected static final byte SPY_MESS = 7;
   protected static final int BYTE = 0;

   protected static final int SHORT = 1;
   protected static final int INT = 2;
   protected static final int LONG = 3;
   protected static final int FLOAT = 4;
   protected static final int DOUBLE = 5;
   protected static final int BOOLEAN = 6;
   protected static final int STRING = 7;
   protected static final int OBJECT = 8;
   protected static final int NULL = 9;

   public static void writeMessage(SpyMessage message, ObjectOutput out) throws IOException {
      if (message instanceof SpyEncapsulatedMessage) {
         out.writeByte(ENCAP_MESS);
      } else if (message instanceof SpyObjectMessage) {
         out.writeByte(OBJECT_MESS);
      } else if (message instanceof SpyBytesMessage) {
         out.writeByte(BYTES_MESS);
      } else if (message instanceof SpyMapMessage) {
         out.writeByte(MAP_MESS);
      } else if (message instanceof SpyTextMessage) {
         out.writeByte(TEXT_MESS);
      } else if (message instanceof SpyStreamMessage) {
         out.writeByte(STREAM_MESS);
      } else {
         out.writeByte(SPY_MESS);
      }
      message.writeExternal(out);
   }

   public static SpyMessage readMessage(ObjectInput in) throws IOException {
      SpyMessage message = null;
      byte type = in.readByte();
      switch (type) {
         case OBJECT_MESS :
            message = MessagePool.getObjectMessage();
            break;
         case BYTES_MESS :
            message = MessagePool.getBytesMessage();
            break;
         case MAP_MESS :
            message = MessagePool.getMapMessage();
            break;
         case STREAM_MESS :
            message = MessagePool.getStreamMessage();
            break;
         case TEXT_MESS :
            message = MessagePool.getTextMessage();
            break;
         case ENCAP_MESS :
            message = MessagePool.getEncapsulatedMessage();
            break;
         default :
            message = MessagePool.getMessage();
      }
      try {
         message.readExternal(in);
      } catch (ClassNotFoundException cnf) {
         throw new IOException("Class not found when reading in spy message.");
      }
      return message;
   }

   public void writeExternal(java.io.ObjectOutput out) throws java.io.IOException {
      SpyDestination.writeDest(out, header.jmsDestination);
      out.writeInt(header.jmsDeliveryMode);
      out.writeLong(header.jmsExpiration);
      out.writeInt(header.jmsPriority);
      writeString(out, header.jmsMessageID);
      out.writeLong(header.jmsTimeStamp);
      out.writeBoolean(header.jmsCorrelationID);
      writeString(out, header.jmsCorrelationIDString);
      if (header.jmsCorrelationIDbyte == null)
         out.writeInt(-1);
      else {
         out.writeInt(header.jmsCorrelationIDbyte.length);
         out.write(header.jmsCorrelationIDbyte);
      }
      SpyDestination.writeDest(out, header.jmsReplyTo);
      writeString(out, header.jmsType);
      out.writeBoolean(header.jmsRedelivered);
      out.writeBoolean(header.jmsPropertiesReadWrite);
      out.writeBoolean(header.msgReadOnly);
      writeString(out, header.producerClientId);
      //write out header.jmsPropertiess
      java.util.Set keys = header.jmsProperties.keySet();
      out.writeInt(keys.size());
      for (java.util.Iterator it = keys.iterator(); it.hasNext();) {
         String key = (String) it.next();
         out.writeUTF(key);
         Object value = header.jmsProperties.get(key);
         if (value == null) {
            out.writeByte(OBJECT);
            out.writeObject(value);
         } else if (value instanceof String) {
            out.writeByte(STRING);
            out.writeUTF((String) value);
         } else if (value instanceof Integer) {
            out.writeByte(INT);
            out.writeInt(((Integer) value).intValue());
         } else if (value instanceof Boolean) {
            out.writeByte(BOOLEAN);
            out.writeBoolean(((Boolean) value).booleanValue());
         } else if (value instanceof Byte) {
            out.writeByte(BYTE);
            out.writeByte(((Byte) value).byteValue());
         } else if (value instanceof Short) {
            out.writeByte(SHORT);
            out.writeShort(((Short) value).shortValue());
         } else if (value instanceof Long) {
            out.writeByte(LONG);
            out.writeLong(((Long) value).longValue());
         } else if (value instanceof Float) {
            out.writeByte(FLOAT);
            out.writeFloat(((Float) value).floatValue());
         } else if (value instanceof Double) {
            out.writeByte(DOUBLE);
            out.writeDouble(((Double) value).doubleValue());
         } else {
            out.writeByte(OBJECT);
            out.writeObject(value);
         }
      }
   }

   private static void writeString(java.io.ObjectOutput out, String s) throws java.io.IOException {
      if (s == null) {
         out.writeByte(NULL);
      } else {
         out.writeByte(STRING);
         out.writeUTF(s);
      }
   }

   private static String readString(java.io.ObjectInput in) throws java.io.IOException {
      byte b = in.readByte();
      if (b == NULL)
         return null;
      else
         return in.readUTF();
   }

   public void readExternal(java.io.ObjectInput in) throws java.io.IOException, ClassNotFoundException {
      header.jmsDestination = SpyDestination.readDest(in);
      header.jmsDeliveryMode = in.readInt();
      header.jmsExpiration = in.readLong();
      header.jmsPriority = in.readInt();
      header.jmsMessageID = readString(in);
      header.jmsTimeStamp = in.readLong();
      header.jmsCorrelationID = in.readBoolean();
      header.jmsCorrelationIDString = readString(in);
      int length = in.readInt();
      if (length < 0)
         header.jmsCorrelationIDbyte = null;
      else {
         header.jmsCorrelationIDbyte = new byte[length];
         in.readFully(header.jmsCorrelationIDbyte);
      }
      header.jmsReplyTo = SpyDestination.readDest(in);
      header.jmsType = readString(in);
      header.jmsRedelivered = in.readBoolean();
      header.jmsPropertiesReadWrite = in.readBoolean();
      header.msgReadOnly = in.readBoolean();
      header.producerClientId = readString(in);
      //read in header.jmsPropertiess
      header.jmsProperties = new HashMap();
      int size = in.readInt();
      for (int i = 0; i < size; i++) {
         String key = in.readUTF();
         byte type = in.readByte();
         Object value = null;
         switch (type) {
            case BYTE :
               value = new Byte(in.readByte());
               break;
            case SHORT :
               value = new Short(in.readShort());
               break;
            case INT :
               value = new Integer(in.readInt());
               break;
            case LONG :
               value = new Long(in.readLong());
               break;
            case FLOAT :
               value = new Float(in.readFloat());
               break;
            case DOUBLE :
               value = new Double(in.readDouble());
               break;
            case BOOLEAN :
               value = new Boolean(in.readBoolean());
               break;
            case STRING :
               value = in.readUTF();
               break;
            default :
               value = in.readObject();
         }
         header.jmsProperties.put(key, value);
      }
   }

   //clear for next use in pool
   void clearMessage() throws JMSException{
      clearBody();
      this.ack = null;
      this.session = null;
      //Set by send() method
      this.header.jmsDestination = null;
      this.header.jmsDeliveryMode = -1;
      this.header.jmsExpiration = 0;
      this.header.jmsPriority = -1;
      this.header.jmsMessageID = null;
      this.header.jmsTimeStamp = 0;
      //Set by the client
      this.header.jmsCorrelationID = true;
      this.header.jmsCorrelationIDString = null;
      this.header.jmsCorrelationIDbyte = null;
      this.header.jmsReplyTo = null;
      this.header.jmsType = null;
      //Set by the provider
      this.header.jmsRedelivered = false;
      //Properties
      this.header.jmsProperties.clear();
      this.header.jmsPropertiesReadWrite=true;
      //Message body
      this.header.msgReadOnly = false;
      //For noLocal to be able to tell if this was a locally produced message
      this.header.producerClientId = null;
      //For durable subscriptions
      this.header.durableSubscriberID = null;
      //For ordering in the JMSServerQueue (set on the server side)
      this.header.messageId = 0;
   }
   
   public String toString() {
      return getClass().getName()+" {\n"+
      	header+"\n"+
      	"}";
   }

}
/*
vim:ts=3:sw=3:et
*/
