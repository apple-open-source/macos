/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.modelmbean;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;
import java.io.StreamCorruptedException;

import javax.management.MBeanNotificationInfo;
import javax.management.Descriptor;
import javax.management.DescriptorAccess;

import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.util.Serialization;

/**
 * Represents a notification in a Model MBean's management interface.
 *
 * @see javax.management.modelmbean.ModelMBeanInfo
 * @see javax.management.modelmbean.ModelMBeanAttributeInfo
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3.8.1 $
 *
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class ModelMBeanNotificationInfo
         extends MBeanNotificationInfo
         implements DescriptorAccess, Cloneable
{

   // Attributes ----------------------------------------------------
   private Descriptor descriptor = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = -5211564525059047097L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("ntfyDescriptor", Descriptor.class)
         };
         break;
      default:
         serialVersionUID = -7445681389570207141L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("notificationDescriptor", Descriptor.class)
         };
      }
   }

   // Constructors --------------------------------------------------
   public ModelMBeanNotificationInfo(String[] notifTypes, String name, String description)
   {
      super(notifTypes, name, description);
      setDescriptor(createDefaultDescriptor());
   }

   public ModelMBeanNotificationInfo(String[] notifTypes, String name, String description,
                                     Descriptor descriptor)
   {
      this(notifTypes, name, description);

      if (descriptor == null || !descriptor.isValid())
         setDescriptor(createDefaultDescriptor());
      else
         setDescriptor(descriptor);
   }

   public ModelMBeanNotificationInfo(ModelMBeanNotificationInfo info)
   {
      this(info.getNotifTypes(), info.getName(), info.getDescription(), info.getDescriptor());
   }

   // Public --------------------------------------------------------
   public Descriptor getDescriptor()
   {
      return (Descriptor)descriptor.clone();
   }

   public void setDescriptor(Descriptor inDescriptor)
   {
      if (inDescriptor == null)
         inDescriptor = createDefaultDescriptor();

      if (!inDescriptor.isValid())
         // FIXME: give more detailed error
         throw new IllegalArgumentException("Invalid descriptor.");

      this.descriptor = inDescriptor;
   }


   // Cloneable implementation --------------------------------------
   public Object clone() throws CloneNotSupportedException
   {
      ModelMBeanNotificationInfo clone = (ModelMBeanNotificationInfo)super.clone();
      clone.descriptor  = (Descriptor)this.descriptor.clone();

      return clone;
   }

   // Object overrides ----------------------------------------------
   public String toString()
   {
      // FIXME: human readable string
      return super.toString();
   }

   // Private -------------------------------------------------------
   private Descriptor createDefaultDescriptor()
   {
      DescriptorSupport descr = new DescriptorSupport();
      descr.setField(ModelMBeanConstants.NAME, super.getName());
      descr.setField(ModelMBeanConstants.DESCRIPTOR_TYPE, ModelMBeanConstants.NOTIFICATION_DESCRIPTOR);
      return descr;
   }

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         descriptor = (Descriptor) getField.get("ntfyDescriptor", null);
         break;
      default:
         descriptor = (Descriptor) getField.get("notificationDescriptor", null);
      }
      if (descriptor == null)
         throw new StreamCorruptedException("Null descriptor?");
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         putField.put("ntfyDescriptor", descriptor);
         break;
      default:
         putField.put("notificationDescriptor", descriptor);
      }
      oos.writeFields();
   }
}




