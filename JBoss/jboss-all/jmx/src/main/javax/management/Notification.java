/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;

import org.jboss.mx.util.Serialization;

/**
 * A Notification.<p>
 *
 * <p><b>Revisions:</b>
 * <p><b>20020329 Adrian Brock:</b>
 * <ul>
 * <li>Make the source serializable
 * </ul>
 * <p><b>20020710 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2.6.2 $
 */
public class Notification
   extends java.util.EventObject
{
   // Constants ---------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields = new ObjectStreamField[]
   {
      new ObjectStreamField("message",        String.class),
      new ObjectStreamField("sequenceNumber", Long.TYPE),
      new ObjectStreamField("source",         Object.class),
      new ObjectStreamField("timeStamp",      Long.TYPE),
      new ObjectStreamField("type",           String.class),
      new ObjectStreamField("userData",       Object.class)
   };

   // Attributes --------------------------------------------------

   /**
    * The notification type
    */
   private String type = null;

   /**
    * The sequence number of the notification
    */
   private long sequenceNumber = 0;

   /**
    * The message of the notification
    */
   private String message = null;

   /**
    * The time of the notification
    */
   private long timeStamp = System.currentTimeMillis();

   /**
    * The user data of the notification
    */
   private Object userData = null;

   /**
    * The source of the notification
    */
   private Object mySource = null;   

   // Static ------------------------------------------------------

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = 1716977971058914352L;
         break;
      default:
         serialVersionUID = -7516092053498031989L;
      }
   }

   // Constructors ------------------------------------------------

   /**
    * Create a new notification
    *
    * @param type the type of the notification
    * @param source the source of the notification
    * @param sequenceNumber the sequence number of the notification
    */
   public Notification(String type,
                       Object source,
                       long sequenceNumber)
   {
      super(source);
      mySource = source;
      this.type = type;
      this.sequenceNumber = sequenceNumber;
      this.timeStamp = System.currentTimeMillis();
   }

   /**
    * Create a new notification
    *
    * @param type the type of the notification
    * @param source the source of the notification
    * @param sequenceNumber the sequence number of the notification
    * @param message the message of the notification
    */
   public Notification(String type,
                       Object source,
                       long sequenceNumber,
                       String message)
   {
      this(type, source, sequenceNumber);
      this.message = message;
      this.timeStamp = System.currentTimeMillis();
   }
   
   /**
    * Create a new notification
    *
    * @param type the type of the notification
    * @param source the source of the notification
    * @param sequenceNumber the sequence number of the notification
    * @param timeStamp the time of the notification
    */
   public Notification(String type,
                       Object source,
                       long sequenceNumber,
                       long timeStamp)
   {
      this(type, source, sequenceNumber);
      this.timeStamp = timeStamp;
   }
   
   /**
    * Create a new notification
    *
    * @param type the type of the notification
    * @param source the source of the notification
    * @param sequenceNumber the sequence number of the notification
    * @param timeStamp the time of the notification
    * @param message the message of the notification
    */
   public Notification(String type,
                       Object source,
                       long sequenceNumber,
                       long timeStamp,
                       String message)
   {
      this(type, source, sequenceNumber, timeStamp);
      this.message = message;
   }

   // Public ------------------------------------------------------
   
   /**
    * Retrieve the source of the notification
    *
    * @return the source
    */
   public Object getSource()
   {
      return mySource;
   }
   
   /**
    * Set the source of the notification<p>
    *
    * The source must be either a object name or a string that can be
    * used to create a valid object name.
    *
    * @param source the new source
    * @exception IllegalArgumentException when the object name is invalid
    */
   public void setSource(Object source) 
      throws IllegalArgumentException
   {
     if (source instanceof String)
     {
        try
        {
           super.source = new ObjectName((String)source);
        }
        catch (MalformedObjectNameException e)
        {
           throw new IllegalArgumentException("malformed object name: " + source);
        }
     }
     else if (source instanceof ObjectName)
     {
        super.source = source;
     }
     else throw new IllegalArgumentException("Notification source must be an object name");
     mySource = super.source;
   }

   /**
    * Retrieve the sequence number of the notification
    *
    * @return the sequence number
    */
   public long getSequenceNumber()
   {
      return sequenceNumber;
   }

   /**
    * Set the sequence number of the notifiction
    *
    * @param sequenceNumber the new sequence number
    */
   public void setSequenceNumber(long sequenceNumber)
   {
      this.sequenceNumber = sequenceNumber;
   }

   /**
    * Retrieve the type of the notification
    *
    * @return the type
    */
   public String getType()
   {
      return type;
   }

   /**
    * Retrieve the time of the notification
    *
    * @return the time
    */
   public long getTimeStamp()
   {
      return timeStamp;
   }

   /**
    * Set the time of the notifiction
    *
    * @param timeStamp the new time
    */
   public void setTimeStamp(long timeStamp)
   {
      this.timeStamp = timeStamp;
   }

   /**
    * Retrieve the message of the notification
    *
    * @return the message
    */
   public String getMessage()
   {
      return message;
   }

   /**
    * Retrieve the user data of the notification
    *
    * @return the user data
    */
   public Object getUserData()
   {
      return userData;
   }

   /**
    * Set the user data of the notifiction
    *
    * @param userData the new user data
    */
   public void setUserData(Object userData)
   {
      this.userData = userData;
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer(getClass().getName());
      tmp.append('[');
      tmp.append("source=");
      tmp.append(this.getSource());
      tmp.append(",type=");
      tmp.append(this.getType());
      tmp.append(",sequenceNumber=");
      tmp.append(this.getSequenceNumber());
      tmp.append(",timeStamp=");
      tmp.append(this.getTimeStamp());
      tmp.append(",message=");
      tmp.append(this.getMessage());
      tmp.append(",userData=");
      tmp.append(this.getUserData());
      tmp.append(']');
      return tmp.toString();
   }

   // X implementation --------------------------------------------

   // Y overrides -------------------------------------------------

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      message = (String) getField.get("message", null);
      sequenceNumber = getField.get("sequenceNumber", 0L);
      mySource = getField.get("source", null);
      timeStamp = getField.get("timeStamp", 0L);
      type = (String) getField.get("type", null);
      userData = getField.get("userData", null);
      source=mySource;
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      putField.put("message", message);
      putField.put("sequenceNumber", sequenceNumber);
      putField.put("source", mySource);
      putField.put("timeStamp", timeStamp);
      putField.put("type", type);
      putField.put("userData", userData);
      oos.writeFields();
   }

   // Inner classes -----------------------------------------------
}

