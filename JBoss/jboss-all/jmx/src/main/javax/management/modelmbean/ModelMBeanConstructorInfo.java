/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.modelmbean;

import java.lang.reflect.Constructor;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;
import java.io.StreamCorruptedException;

import java.util.Arrays;

import javax.management.Descriptor;
import javax.management.DescriptorAccess;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanParameterInfo;

import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.util.Serialization;

/**
 * Represents constructor.
 *
 * @see javax.management.modelmbean.ModelMBeanInfo
 * @see javax.management.modelmbean.ModelMBeanInfoSupport
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 * @version $Revision: 1.3.8.1 $
 *
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 *   
 */
public class ModelMBeanConstructorInfo
   extends MBeanConstructorInfo
   implements DescriptorAccess, Cloneable
{

   // Attributes ----------------------------------------------------
   
   /**
    * The descriptor associated with this constructor.
    */
   private Descriptor descriptor = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = -4440125391095574518L;
         break;
      default:
         serialVersionUID = 3862947819818064362L;
      }
      serialPersistentFields = new ObjectStreamField[]
      {
         new ObjectStreamField("consDescriptor", Descriptor.class)
      };
   }
   
   // Constructors --------------------------------------------------
   /**
    * Creates a new constructor info with a default descriptor.
    *
    * @param   description human readable description string
    * @param   constructorMethod a <tt>Constructor</tt> instance representing the MBean constructor
    */
   public ModelMBeanConstructorInfo(String description, Constructor constructorMethod)
   {
      super(description, constructorMethod);
      setDescriptor(createDefaultDescriptor());
   }

   /**
    * Creates a new constructor info with a given descriptor. If a <tt>null</tt> or invalid descriptor
    * is passed as a parameter, a default descriptor will be created for the constructor.
    *
    * @param   description human readable description string
    * @param   constructorMethod a <tt>Constructor</tt> instance representing the MBean constructor
    * @param   descriptor a descriptor to associate with this constructor
    */
   public ModelMBeanConstructorInfo(String description, Constructor constructorMethod, Descriptor descriptor)
   {
      this(description, constructorMethod);
      
      if (descriptor == null || !descriptor.isValid())
         setDescriptor(createDefaultDescriptor());
      else 
         setDescriptor(descriptor);
   }

   /**
    * Creates a new constructor info with default descriptor.
    *
    * @param   name  name for the constructor
    * @param   description human readable description string
    * @param   signature constructor signature
    */
   public ModelMBeanConstructorInfo(String name, String description, MBeanParameterInfo[] signature)
   {
      super(name, description, signature);
      setDescriptor(createDefaultDescriptor());
   }

   /**
    * Creates a new constructor info with a given descriptor. If a <tt>null</tt> or invalid descriptor
    * is passed as a parameter, a default descriptor will be created for the constructor.
    *
    * @param name name for the constructor
    * @param description human readable description string
    * @param signature constructor signature
    * @param descriptor a descriptor to associate with this constructor
    */
   public ModelMBeanConstructorInfo(String name, String description, MBeanParameterInfo[] signature,
                                    Descriptor descriptor)
   {
      this(name, description, signature);
      
      if (descriptor == null || !descriptor.isValid())
         setDescriptor(createDefaultDescriptor());
      else
         setDescriptor(descriptor);
   }
   
   // DescriptorAccess implementation -------------------------------
   
   /**
    * Returns a copy of the descriptor associated with this constructor.
    *
    * @return a copy of this constructor's descriptor instance
    */
   public Descriptor getDescriptor()
   {
      return (Descriptor)descriptor.clone();
   }
   
   /**
    * Replaces the descriptor associated with this constructor. If the <tt>inDescriptor</tt>
    * argument is <tt>null</tt> then the existing descriptor is replaced with a default
    * descriptor.
    *
    * @param   inDescriptor   descriptor used for replacing the existing constructor descriptor
    * @throws  IllegalArgumentException if the new descriptor is not valid
    */
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
   
   public synchronized Object clone() throws CloneNotSupportedException
   {
      ModelMBeanConstructorInfo clone = (ModelMBeanConstructorInfo)super.clone();
      clone.descriptor = (Descriptor)this.descriptor.clone();
      
      return clone;
   }
   
   // Object overrides ----------------------------------------------

   /**
    * @returns a human readable string
    */
   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(getClass().getName()).append(":");
      buffer.append(" name=").append(getName());
      buffer.append(" description=").append(getDescription());
      buffer.append(" signature=").append(Arrays.asList(signature));
      buffer.append(" descriptor=").append(descriptor);
      return buffer.toString();
   }
   
   // Private -------------------------------------------------------
   private Descriptor createDefaultDescriptor()
   {
      DescriptorSupport descr = new DescriptorSupport();
      descr.setField(ModelMBeanConstants.NAME, super.getName());
      descr.setField(ModelMBeanConstants.DESCRIPTOR_TYPE, ModelMBeanConstants.CONSTRUCTOR_DESCRIPTOR);
      descr.setField(ModelMBeanConstants.ROLE, ModelMBeanConstants.CONSTRUCTOR);
      
      // FIXME: check the spec for all mandatory fields!
      
      return descr;
   }

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      descriptor = (Descriptor) getField.get("consDescriptor", null);
      if (descriptor == null)
         throw new StreamCorruptedException("Null descriptor?");
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      putField.put("consDescriptor", descriptor);
      oos.writeFields();
   }
   
}
