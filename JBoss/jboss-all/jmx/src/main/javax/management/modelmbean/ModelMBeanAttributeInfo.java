/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.modelmbean;

import java.lang.reflect.Method;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamField;
import java.io.StreamCorruptedException;

import javax.management.Descriptor;
import javax.management.DescriptorAccess;
import javax.management.MBeanAttributeInfo;
import javax.management.IntrospectionException;

import org.jboss.logging.Logger;
import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.util.Serialization;

/**
 * Represents a Model MBean's management attribute.
 *
 * @see javax.management.MBeanAttributeInfo
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 * @version $Revision: 1.6.4.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20020320 Juha Lindfors:</b>
 * <ul>
 * <li>toString() implementation</li>
 *
 * <li>Changed the default descriptor to include field <tt>currencyTimeLimit</tt>
 *     with a value -1. Since default descriptors do not include method mapping
 *     this automatically caches attribute values in the Model MBean.
 * </li>
 * </ul>
 *
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class ModelMBeanAttributeInfo
   extends MBeanAttributeInfo
   implements DescriptorAccess, Cloneable
{

   // Attributes ----------------------------------------------------
   /**
    * The descriptor associated with this attribute.
    */
   private Descriptor descriptor = null;

   // Static --------------------------------------------------------

   private static final Logger log = Logger.getLogger(ModelMBeanAttributeInfo.class);

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = 7098036920755973145L;
         break;
      default:
         serialVersionUID = 6181543027787327345L;
      }
      serialPersistentFields = new ObjectStreamField[]
      {
         new ObjectStreamField("attrDescriptor", Descriptor.class)
      };
   }

   // Constructors --------------------------------------------------
   /**
    * Creates a new attribute info with a default descriptor.
    *
    * @param  name name of the attribute
    * @param  description human readable description string
    * @param  getter a <tt>Method</tt> instance representing a read method for this attribute
    * @param  setter a <tt>Method</tt> instance representing a write method for this attribute
    *
    * @throws IntrospectionException if the accessor methods are not valid for this attribute
    */
   public ModelMBeanAttributeInfo(String name, String description, Method getter, Method setter)
         throws IntrospectionException
   {
      // NOTE:  This constructor provides the method mapping for
      //        a Model MBean attribute so the 'setMethod' and 'getMethod'
      //        descriptor field should be set, but the javadoc dictates a
      //        default descriptor...?
      //
      // TODO:  We should go against the RI javadoc and create a descriptor
      //        with the proper method mapping. [JPL]
      
      super(name, description, getter, setter);
      setDescriptor(createDefaultDescriptor());
   }

   /**
    * Creates a new attribute info object. If a <tt>null</tt> or
    * invalid descriptor is passed as a parameter, a default descriptor will be created
    * for the attribute.
    *
    * @param  name name of the attribute
    * @param  description human readable description string
    * @param  getter a <tt>Method</tt> instance representing a read method for this attribute
    * @param  setter a <tt>Method</tt> instance representing a write method for this attribute
    * @param  descriptor a descriptor to associate with this attribute
    *
    * @throws IntrospectionException if the accessor methods are not valid for this attribute
    */
   public ModelMBeanAttributeInfo(String name, String description, Method getter, Method setter, Descriptor descriptor)
         throws IntrospectionException
   {
      this(name, description, getter, setter);

      if (descriptor == null || !descriptor.isValid())
         setDescriptor(createDefaultDescriptor());
      else
         setDescriptor(descriptor);
   }

   /**
    * Creates a new attribute info object with a default descriptor.
    *
    * @param   name  name of the attribute
    * @param   type  fully qualified class name of the attribute's type
    * @param   description human readable description string
    * @param   isReadable true if attribute is readable; false otherwise
    * @param   isWritable true if attribute is writable; false otherwise
    * @param   isIs (not used for Model MBeans; false)
    */
   public ModelMBeanAttributeInfo(String name, String type, String description,
                                  boolean isReadable, boolean isWritable, boolean isIs)
   {
      // JPL:  As far as I can tell, the isIs boolean has no use in the Model MBean
      //       attribute info (since attributes will map to methods through operations)
      //       I'm setting this boolean to false, until someone complains.

      super(name, type, description, isReadable, isWritable, false /*isIs*/);
      setDescriptor(createDefaultDescriptor());

      if (isIs == true)
         log.warn("WARNING: supplied isIS=true, set to false");
   }

   /**
    * Creates a new attribute info object with a given descriptor. If a <tt>null</tt> or invalid
    * descriptor is passed as a parameter, a default descriptor will be created for the attribute.
    *
    * @param  name   name of the attribute
    * @param  type   fully qualified class name of the attribute's type
    * @param  description human readable description string
    * @param  isReadable true if the attribute is readable; false otherwise
    * @param  isWritable true if the attribute is writable; false otherwise
    * @param  isIs  (not used for Model MBeans; false)
    */
   public ModelMBeanAttributeInfo(String name, String type, String description,
                                  boolean isReadable, boolean isWritable, boolean isIs, Descriptor descriptor)
   {
      // JPL:  As far as I can tell, the isIs boolean has no use in the Model MBean
      //       attribute info (since attributes will map to methods through operations)
      //       I'm setting this boolean to false, until someone complains.
      super(name, type, description, isReadable, isWritable, false/*isIs*/);

      if (descriptor == null || !descriptor.isValid())
         setDescriptor(createDefaultDescriptor());
      else
         setDescriptor(descriptor);

      if (isIs == true)
         log.warn("WARNING: supplied isIS=true, set to false");
   }

   /**
    * Copy constructor.
    *
    * @param   inInfo the attribute info to copy
    */
   public ModelMBeanAttributeInfo(ModelMBeanAttributeInfo info)
   {
      // THS - javadoc says a default descriptor will be created but that's not
      // consistent with the other *Info classes.
      // I'm also assuming that getDescriptor returns a clone.
      this(info.getName(), info.getType(), info.getDescription(), info.isReadable(),
           info.isWritable(), info.isIs(), info.getDescriptor());
   }

   // DescriptorAccess implementation -------------------------------
   /**
    * Returns a copy of the descriptor associated with this attribute.
    *
    * @return a copy of this attribute's descriptor
    */
   public Descriptor getDescriptor()
   {
      return (Descriptor)descriptor.clone();
   }

   /**
    * Replaces the descriptor associated with this attribute. If the <tt>inDescriptor</tt>
    * argument is <tt>null</tt> then the existing descriptor is replaced with a default
    * descriptor.
    *
    * @param   inDescriptor   descriptor used for replacing the existing operation descriptor
    * @throws IllegalArgumentException if the new descriptor is not valid
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
   /**
    * Creates a copy of this object.
    *
    * @return clone of this object
    * @throws CloneNotSupportedException if there was a failure creating the copy
    */
   public synchronized Object clone() throws CloneNotSupportedException
   {
      ModelMBeanAttributeInfo clone = (ModelMBeanAttributeInfo)super.clone();
      clone.descriptor  = (Descriptor)this.descriptor.clone();

      return clone;
   }

   // Object override -----------------------------------------------
   /**
    * Returns a string representation of this Model MBean attribute info object.
    * The returned string is in the form: <pre>
    *
    *   ModelMBeanAttributeInfo[Name=&lt;attribute name&gt;,
    *   Type=&lt;class name of the attribute type&gt;,
    *   Access= RW | RO | WO,
    *   Descriptor=(fieldName1=fieldValue1, ... , fieldName&lt;n&gt;=fieldValue&lt;n&gt;)]
    *
    * </pre>
    *
    * @return string representation of this object
    */
   public String toString()
   {
      return "ModelMBeanAttributeInfo[" +
             "Name=" + getName() +
             ",Type=" + getType() +
             ",Access=" + ((isReadable() && isWritable()) ? "RW" : (isReadable()) ? "RO" : "WO") +
             ",Descriptor(" + getDescriptor() + ")]";
   }

   // Private -------------------------------------------------------
   
   /**
    * Creates a default descriptor with <tt>"name"</tt> field set to this
    * attributes name, <tt>"descriptorType"</tt> field set to <tt>"attribute"</tt>
    * and <tt>"currencyTimeLimit"</tt> set to -1 (always cache attributes).
    *
    * @return  a default descriptor
    */
   private Descriptor createDefaultDescriptor()
   {
      DescriptorSupport descr = new DescriptorSupport();
      descr.setField(ModelMBeanConstants.NAME, super.getName());
      descr.setField(ModelMBeanConstants.DESCRIPTOR_TYPE, ModelMBeanConstants.ATTRIBUTE_DESCRIPTOR);

      // [JPL] Setting the currencyTimeLimit to -1 for default descriptors.
      //       This means that for attributes the resource object is never
      //       invoked. For default descriptors this is the correct behavior
      //       since method mapping to the resource (setMethod, getMethod)
      //       does not exist. However, it means that if the descriptor is
      //       changed afterwards with method mapping, the currencyTimeLimit
      //       property should be changed as well (otherwise the methods on 
      //       the resource won't be invoked).
      descr.setField(ModelMBeanConstants.CURRENCY_TIME_LIMIT, "-1");
      
      // FIXME: check the spec for all required descriptor fields!

      return descr;
   }

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      descriptor = (Descriptor) getField.get("attrDescriptor", null);
      if (descriptor == null)
         throw new StreamCorruptedException("Null descriptor?");
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      putField.put("attrDescriptor", descriptor);
      oos.writeFields();
   }

}

