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
import java.io.Serializable;
import java.io.StreamCorruptedException;

import java.util.Map;
import java.util.HashMap;
import java.util.List;
import java.util.ArrayList;

import javax.management.MBeanInfo;
import javax.management.Descriptor;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanException;
import javax.management.RuntimeOperationsException;

import javax.management.modelmbean.ModelMBeanAttributeInfo;
import javax.management.modelmbean.ModelMBeanConstructorInfo;
import javax.management.modelmbean.ModelMBeanOperationInfo;
import javax.management.modelmbean.ModelMBeanNotificationInfo;
import javax.management.modelmbean.ModelMBeanInfo;

import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.util.Serialization;

/**
 * Support class for <tt>ModelMBeanInfo</tt> interface.
 *
 * @see javax.management.modelmbean.ModelMBeanInfo
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 * @version $Revision: 1.9.4.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20020319 Juha Lindfors:</b>
 * <ul>
 * <li>Fixed ArrayIndexOutOfBoundsException bug in getOperationDescriptors()</li>
 * </ul>
 *
 * <p><b>20020525 Juha Lindfors:</b>
 * <ul>
 * <li>Fixed the getDescriptor(name, type) exception behavior to match RI 1.0
 *     javadoc: RuntimeOperationsException is thrown in case of a illegal
 *     descriptor type string.
 * </li>
 * <li> setDescriptors() now implemented and no longer throws 'NYI' error. </li>
 * <li> clone() implemented</li>
 * </ul>
 *
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class ModelMBeanInfoSupport
      extends MBeanInfo
      implements ModelMBeanInfo, Serializable
{

   // Attributes ----------------------------------------------------
   
   /**
    * MBean descriptor for this Model MBean.
    */
   private Descriptor mbeanDescriptor  = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID;
   private static final ObjectStreamField[] serialPersistentFields;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = -3944083498453227709L;
         // REVIEW: This is not in the spec, constructed from exceptions in testing
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("mmbAttributes", new MBeanAttributeInfo[0].getClass()),
            new ObjectStreamField("mmbConstructors", new MBeanConstructorInfo[0].getClass()),
            new ObjectStreamField("mmbNotifications", new MBeanNotificationInfo[0].getClass()),
            new ObjectStreamField("mmbOperations", new MBeanOperationInfo[0].getClass()),
            new ObjectStreamField("modelMBeanDescriptor", Descriptor.class)
         };
         break;
      default:
         serialVersionUID = -1935722590756516193L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("modelMBeanAttributes", new MBeanAttributeInfo[0].getClass()),
            new ObjectStreamField("modelMBeanConstructors", new MBeanConstructorInfo[0].getClass()),
            new ObjectStreamField("modelMBeanNotifications", new MBeanNotificationInfo[0].getClass()),
            new ObjectStreamField("modelMBeanOperations", new MBeanOperationInfo[0].getClass()),
            new ObjectStreamField("modelMBeanDescriptor", Descriptor.class)
         };
      }
   }
   
   // Constructors --------------------------------------------------
   
   /**
    * Copy constructor for Model MBean info. This instance is initialized with
    * the values of the given Model MBean info.
    *
    * @param   mbi  Model MBean info used to initialize this instance
    */
   public ModelMBeanInfoSupport(ModelMBeanInfo mbi)
   {
      super(mbi.getClassName(), mbi.getDescription(), mbi.getAttributes(),
            mbi.getConstructors(), mbi.getOperations(), mbi.getNotifications());

      try
      {
         setMBeanDescriptor(mbi.getMBeanDescriptor());
      }
      catch (MBeanException e)
      {
         throw new IllegalArgumentException(e.toString() /* FIXME: message */ );
      }
   }

   /**
    * Creates an instance of Model MBean info implementation based on the given
    * values. The Model MBean is configured with a default MBean descriptor.
    *
    * @param   className    name of the Model MBean implementation class
    * @param   description  human readable description string for this Model MBean
    * @param   attributes   an array of Model MBean attribute metadata to describe
    *                       the management attributes of this Model MBean
    * @param   constructors an array of Model MBean constructor metadata that
    *                       describes the constructors of this Model MBean
    *                       implementation class
    * @param   operations   an array of Model MBean operation metadata to describe
    *                       the management operations of this Model MBean
    * @param   notifications an array of Model MBean notification metadata to
    *                        describe the management notifications of this 
    *                        Model MBean
    */
   public ModelMBeanInfoSupport(String className, String description,
                                ModelMBeanAttributeInfo[] attributes,
                                ModelMBeanConstructorInfo[] constructors,
                                ModelMBeanOperationInfo[] operations,
                                ModelMBeanNotificationInfo[] notifications)
   {
      /*
       * NOTE: the use of the className argument is somewhat unclear and it
       *       may later change to describe the class name of the resource class
       *       the Model MBean represents. At the moment though the implementation
       *       matches the JMX 1.0 RI javadoc which states that the class name
       *       is the name of the Model MBean class implementation. The same
       *       applies to the constructor metadata, it currently describes the
       *       the constructors of the ModelMBean class implementation. It is also
       *       unclear from the spec whether the default generic and attribute
       *       change notification should be automatically added to the notification
       *       metadata or if its the responsibility of the client to explicitly
       *       declare them as well.   [JPL]
       */
      super(className, description,
            (null == attributes) ? new ModelMBeanAttributeInfo[0] : attributes,
            (null == constructors) ? new ModelMBeanConstructorInfo[0] : constructors,
            (null == operations) ? new ModelMBeanOperationInfo[0] : operations,
            (null == notifications) ? new ModelMBeanNotificationInfo[0] : notifications);

      try
      {
         setMBeanDescriptor(createDefaultDescriptor(className));
      }
      catch (MBeanException e)
      {
         throw new IllegalArgumentException(e.toString());
      }
   }

   /**
    * Creates an instance of Model MBean info implementation based on the given
    * values and descriptor.
    *
    * @param   className    name of the Model MBean implementation class
    * @param   description  human readable description string for this Model MBean
    * @param   attributes   an array of Model MBean attribute metadata to describe
    *                       the management attributes of this Model MBean
    * @param   constructors an array of Model MBean constructor metadata that
    *                       describes the constructors of this Model MBean
    *                       implementation class
    * @param   operations   an array of Model MBean operation metadata to describe
    *                       the management operations of this Model MBean
    * @param   notifications an array of Model MBean notification metadata to
    *                        describe the management notifications of this 
    *                        Model MBean
    * @param   mbeanDescriptor descriptor for the MBean
    */
   public ModelMBeanInfoSupport(String className, String description,
                                ModelMBeanAttributeInfo[] attributes,
                                ModelMBeanConstructorInfo[] constructors,
                                ModelMBeanOperationInfo[] operations,
                                ModelMBeanNotificationInfo[] notifications,
                                Descriptor mbeandescriptor)
   {
      this(className, description, attributes, constructors, operations, notifications);
      
      try
      {
         setMBeanDescriptor(mbeandescriptor);
      }
      catch (MBeanException e)
      {
         throw new IllegalArgumentException(e.toString());
      }
   }


   // ModelMBeanInfo interface implementation -----------------------
   
   /**
    * Returns the descriptors of an Model MBean for a given management
    * interface element type. The descriptor type must be one of the following:  <br><pre>
    *
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#MBEAN_DESCRIPTOR MBEAN_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#ATTRIBUTE_DESCRIPTOR ATTRIBUTE_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#OPERATION_DESCRIPTOR OPERATION_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#NOTIFICATION_DESCRIPTOR NOTIFICATION_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#CONSTRUCTOR_DESCRIPTOR CONSTRUCTOR_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#ALL_DESCRIPTORS ALL_DESCRIPTORS}
    *
    * </pre>
    * 
    * Using <tt>ALL_DESCRIPTORS</tt> returns descriptors for the MBean, and all
    * its attributes, operations, notifications and constructors.
    *
    * @param   descrType   descriptor type string
    * 
    * @return  MBean descriptors.
    */
   public Descriptor[] getDescriptors(String descrType) throws MBeanException
   {
      if (descrType == null)
      {
         List list = new ArrayList(100);
         list.add(mbeanDescriptor);
         list.addAll(getAttributeDescriptors().values());
         list.addAll(getOperationDescriptors().values());
         list.addAll(getNotificationDescriptors().values());
         list.addAll(getConstructorDescriptors().values());
         return (Descriptor[])list.toArray(new Descriptor[0]);
      }

      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.MBEAN_DESCRIPTOR))
         return new Descriptor[] { mbeanDescriptor };

      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.ATTRIBUTE_DESCRIPTOR))
         return (Descriptor[])getAttributeDescriptors().values().toArray(new Descriptor[0]);

      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.OPERATION_DESCRIPTOR))
         return (Descriptor[])getOperationDescriptors().values().toArray(new Descriptor[0]);

      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.NOTIFICATION_DESCRIPTOR))
         return (Descriptor[])getNotificationDescriptors().values().toArray(new Descriptor[0]);

      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.CONSTRUCTOR_DESCRIPTOR))
         return (Descriptor[])getConstructorDescriptors().values().toArray(new Descriptor[0]);

      throw new IllegalArgumentException("unknown descriptor type: " + descrType);
   }

   /**
    * Returns a descriptor of a management interface element matching the given
    * name and type. The descriptor type string must be one of the following:   <br><pre>
    *
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#MBEAN_DESCRIPTOR MBEAN_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#ATTRIBUTE_DESCRIPTOR ATTRIBUTE_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#OPERATION_DESCRIPTOR OPERATION_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#NOTIFICATION_DESCRIPTOR NOTIFICATION_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#CONSTRUCTOR_DESCRIPTOR CONSTRUCTOR_DESCRIPTOR}
    *
    * </pre>
    *
    * @param   descrName   name of the descriptor
    * @param   descrType   type of the descriptor
    *
    * @return  the requested descriptor or <tt>null</tt> if it was not found
    *
    * @throws  RuntimeOperationsException if an illegal descriptor type was given 
    */
   public Descriptor getDescriptor(String descrName, String descrType) throws MBeanException
   {
      if (descrType == null)
         throw new RuntimeOperationsException(new IllegalArgumentException("null descriptor type"));

      if (descrType.equalsIgnoreCase(ModelMBeanConstants.MBEAN_DESCRIPTOR))
         return mbeanDescriptor;
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.ATTRIBUTE_DESCRIPTOR))
         return (Descriptor)getAttributeDescriptors().get(descrName);
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.OPERATION_DESCRIPTOR))
         return (Descriptor)getOperationDescriptors().get(descrName);
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.CONSTRUCTOR_DESCRIPTOR))
         return (Descriptor)getConstructorDescriptors().get(descrName);
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.NOTIFICATION_DESCRIPTOR))
         return (Descriptor)getNotificationDescriptors().get(descrName);

      throw new RuntimeOperationsException(new IllegalArgumentException("unknown descriptor type: " + descrType));
   }


   /**
    * Adds or replaces the descriptors in this Model MBean. All descriptors
    * must be valid. <tt>Null</tt> references will be ignored.
    *
    * @param   inDescriptors  array of descriptors
    */
   public void setDescriptors(Descriptor[] inDescriptors) throws MBeanException
   {
      for (int i = 0; i < inDescriptors.length; ++i)
      {
         if (inDescriptors[i] != null && inDescriptors[i].isValid())
         {
            setDescriptor(
                  inDescriptors[i],  
                  (String)inDescriptors[i].getFieldValue(ModelMBeanConstants.DESCRIPTOR_TYPE)
            );
         }
      }
   }
   
   /**
    * Adds or replaces the descriptor in this Model MBean. Descriptor must be
    * valid. If <tt>descrType</tt> is not specified, the <tt>descriptorType</tt>
    * field of the given descriptor is used.   <p>
    *
    * The <tt>descriptorType</tt> must contain one of the following values:   <br><pre>
    *
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#MBEAN_DESCRIPTOR MBEAN_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#ATTRIBUTE_DESCRIPTOR ATTRIBUTE_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#OPERATION_DESCRIPTOR OPERATION_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#NOTIFICATION_DESCRIPTOR NOTIFICATION_DESCRIPTOR}
    *   - {@link org.jboss.mx.modelmbean.ModelMBeanConstants#CONSTRUCTOR_DESCRIPTOR CONSTRUCTOR_DESCRIPTOR}
    *
    * </pre>
    *
    * @param   descr     descriptor to set
    * @param   descrType descriptor type string, can be <tt>null</tt>
    *
    * @throws RuntimeOperationsException if <tt>descr</tt> is <tt>null</tt>, or
    *         descriptor is not valid.
    */
   public void setDescriptor(Descriptor descr, String descrType) throws MBeanException
   {
      if (descr == null)
         throw new RuntimeOperationsException(new IllegalArgumentException("null descriptor"));
         
      if (!descr.isValid())
         throw new RuntimeOperationsException(new IllegalArgumentException("not a valid descriptor"));
         
      if (descrType == null)
         descrType = (String)descr.getFieldValue(ModelMBeanConstants.DESCRIPTOR_TYPE);
         
      if (descrType.equalsIgnoreCase(ModelMBeanConstants.MBEAN_DESCRIPTOR))
      {
         setMBeanDescriptor(descr);
      }
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.ATTRIBUTE_DESCRIPTOR))
      {
         ModelMBeanAttributeInfo info = getAttribute((String)descr.getFieldValue(ModelMBeanConstants.NAME));
         info.setDescriptor(descr);
      }
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.OPERATION_DESCRIPTOR))
      {
         ModelMBeanOperationInfo info = getOperation((String)descr.getFieldValue(ModelMBeanConstants.NAME));
         info.setDescriptor(descr);
      }
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.CONSTRUCTOR_DESCRIPTOR))
      {
         ModelMBeanConstructorInfo info = getConstructor((String)descr.getFieldValue(ModelMBeanConstants.NAME));
         info.setDescriptor(descr);
      }
      else if (descrType.equalsIgnoreCase(ModelMBeanConstants.NOTIFICATION_DESCRIPTOR))
      {
         ModelMBeanNotificationInfo info = getNotification((String)descr.getFieldValue(ModelMBeanConstants.NAME));
         info.setDescriptor(descr);
      }
      else
         throw new RuntimeOperationsException(new IllegalArgumentException("unknown descriptor type: " + descrType));
   }

   public ModelMBeanAttributeInfo getAttribute(String inName) throws MBeanException
   {
      for (int i = 0; i < attributes.length; ++i)
         if (attributes[i].getName().equals(inName))
            return (ModelMBeanAttributeInfo)attributes[i];

      throw new RuntimeOperationsException(new IllegalArgumentException("MBean does not contain attribute " + inName));
   }

   public ModelMBeanOperationInfo getOperation(String inName) throws MBeanException
   {
      for (int i = 0; i < operations.length; ++i)
         if (operations[i].getName().equals(inName))
            return (ModelMBeanOperationInfo)operations[i];

      throw new RuntimeOperationsException(new IllegalArgumentException("MBean does not contain operation " + inName));
   }

   public ModelMBeanConstructorInfo getConstructor(String inName) throws MBeanException
   {
      for (int i = 0; i < constructors.length; ++i)
         if (constructors[i].getName().equals(inName))
            return (ModelMBeanConstructorInfo)constructors[i];

      throw new RuntimeOperationsException(new IllegalArgumentException("MBean does not contain constructor " + inName));
   }

   public ModelMBeanNotificationInfo getNotification(String inName) throws MBeanException
   {
      for (int i = 0; i < notifications.length; ++i)
         if (notifications[i].getName().equals(inName))
            return (ModelMBeanNotificationInfo)notifications[i];

      throw new RuntimeOperationsException(new IllegalArgumentException("MBean does not contain notification " + inName));
   }

   public MBeanAttributeInfo[] getAttributes()
   {
      return super.getAttributes();
   }

   public MBeanOperationInfo[] getOperations()
   {
      return super.getOperations();
   }

   public MBeanConstructorInfo[] getConstructors()
   {
      return super.getConstructors();
   }

   public MBeanNotificationInfo[] getNotifications()
   {
      return super.getNotifications();
   }

   public Descriptor getMBeanDescriptor() throws MBeanException
   {
      return mbeanDescriptor;
   }

   public void setMBeanDescriptor(Descriptor inMBeanDescriptor) throws MBeanException
   {
      Descriptor descr = (Descriptor)inMBeanDescriptor.clone();
      addDefaultMBeanDescriptorFields(descr);

      this.mbeanDescriptor = descr;
   }

   // Public --------------------------------------------------------
   
   /**
    * @deprecated use {@link #getDescriptor(String, String)} instead.
    */
   public Descriptor getDescriptor(String descrName) throws MBeanException
   {
      
      /*
       * NOTE:  this method is not part of the ModelMBeanInfo interface but is
       *        included in the RI javadocs so it is also here for the sake
       *        of completeness. The problem here though is that this method
       *        to work without the descriptor type string assumes unique name
       *        for all descriptors regardless their type (something that is
       *        not mandated by the spec). Hence the deprecated tag.   [JPL]
       */
      if (descrName.equals(mbeanDescriptor.getFieldValue(ModelMBeanConstants.NAME)))
         return mbeanDescriptor;

      Descriptor descr = null;

      descr = (Descriptor)getAttributeDescriptors().get(descrName);
      if (descr != null)
         return descr;

      descr = (Descriptor)getOperationDescriptors().get(descrName);
      if (descr != null)
         return descr;

      descr = (Descriptor)getNotificationDescriptors().get(descrName);
      if (descr != null)
         return descr;

      descr = (Descriptor)getConstructorDescriptors().get(descrName);
      if (descr != null)
         return descr;

      return null;
   }

   
   // Y overrides ---------------------------------------------------
   public synchronized Object clone()
   {
      try
      {
         ModelMBeanInfoSupport clone = (ModelMBeanInfoSupport)super.clone();
         clone.mbeanDescriptor = (Descriptor)mbeanDescriptor.clone();

         return clone;
      }
      catch (CloneNotSupportedException e)
      {
         return null;
      }
   }

   // Private -------------------------------------------------------
   private void addDefaultMBeanDescriptorFields(Descriptor descr)
   {
      if (descr.getFieldValue(ModelMBeanConstants.NAME) == null || descr.getFieldValue(ModelMBeanConstants.NAME).equals(""))
         descr.setField(ModelMBeanConstants.NAME, className);
      if (descr.getFieldValue(ModelMBeanConstants.DESCRIPTOR_TYPE) == null)
         descr.setField(ModelMBeanConstants.DESCRIPTOR_TYPE, ModelMBeanConstants.MBEAN_DESCRIPTOR);
      if (!(((String)descr.getFieldValue(ModelMBeanConstants.DESCRIPTOR_TYPE)).equalsIgnoreCase(ModelMBeanConstants.MBEAN_DESCRIPTOR)))
         descr.setField(ModelMBeanConstants.DESCRIPTOR_TYPE, ModelMBeanConstants.MBEAN_DESCRIPTOR);
      if (descr.getFieldValue(ModelMBeanConstants.DISPLAY_NAME) == null)
         descr.setField(ModelMBeanConstants.DISPLAY_NAME, className);
      if (descr.getFieldValue(ModelMBeanConstants.PERSIST_POLICY) == null)
         descr.setField(ModelMBeanConstants.PERSIST_POLICY, ModelMBeanConstants.NEVER);
      if (descr.getFieldValue(ModelMBeanConstants.LOG) == null)
         descr.setField(ModelMBeanConstants.LOG, "F");
      if (descr.getFieldValue(ModelMBeanConstants.EXPORT) == null)
         descr.setField(ModelMBeanConstants.EXPORT, "F");
      if (descr.getFieldValue(ModelMBeanConstants.VISIBILITY) == null)
         descr.setField(ModelMBeanConstants.VISIBILITY, ModelMBeanConstants.HIGH_VISIBILITY);
   }

   private Descriptor createDefaultDescriptor(String className) {

      return new DescriptorSupport(new String[] {
            ModelMBeanConstants.NAME            + "=" + className,
            ModelMBeanConstants.DESCRIPTOR_TYPE + "=" + ModelMBeanConstants.MBEAN_DESCRIPTOR,
            ModelMBeanConstants.DISPLAY_NAME    + "=" + className,
            ModelMBeanConstants.PERSIST_POLICY  + "=" + ModelMBeanConstants.NEVER,
            ModelMBeanConstants.LOG             + "=" + "F",
            ModelMBeanConstants.EXPORT          + "=" + "F",
            ModelMBeanConstants.VISIBILITY      + "=" + ModelMBeanConstants.HIGH_VISIBILITY
      });
   }

   private Map getAttributeDescriptors()
   {
      Map map = new HashMap();

      for (int i = 0; i < attributes.length; ++i)
      {
         map.put(attributes[i].getName(), ((ModelMBeanAttributeInfo)attributes[i]).getDescriptor());
      }

      return map;
   }

   private Map getOperationDescriptors()
   {
      Map map = new HashMap();

      for (int i = 0; i < operations.length; ++i)
      {
         map.put(operations[i].getName(), ((ModelMBeanOperationInfo)operations[i]).getDescriptor());
      }

      return map;
   }

   private Map getConstructorDescriptors()
   {
      Map map = new HashMap();

      for (int i = 0; i < constructors.length; ++i)
      {
         map.put(constructors[i].getName(), ((ModelMBeanConstructorInfo)constructors[i]).getDescriptor());
      }

      return map;
   }

   private Map getNotificationDescriptors()
   {
      Map map = new HashMap();

      for (int i = 0; i < notifications.length; ++i)
      {
         map.put(notifications[i].getName(), ((ModelMBeanNotificationInfo)notifications[i]).getDescriptor());
      }

      return map;
   }


   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      MBeanAttributeInfo[] attrInfo;
      MBeanConstructorInfo[] consInfo;
      MBeanOperationInfo[] operInfo;
      MBeanNotificationInfo[] ntfyInfo;
      Descriptor desc;

      ObjectInputStream.GetField getField = ois.readFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         attrInfo = (MBeanAttributeInfo[]) getField.get("mmbAttributes", null);
         consInfo = (MBeanConstructorInfo[]) getField.get("mmbConstructors", null);
         ntfyInfo = (MBeanNotificationInfo[]) getField.get("mmbNotifications", null);
         operInfo = (MBeanOperationInfo[]) getField.get("mmbOperations", null);
         break;
      default:
         attrInfo = (MBeanAttributeInfo[]) getField.get("modelMBeanAttributes", null);
         consInfo = (MBeanConstructorInfo[]) getField.get("modelMBeanConstructors", null);
         ntfyInfo = (MBeanNotificationInfo[]) getField.get("modelMBeanNotifications", null);
         operInfo = (MBeanOperationInfo[]) getField.get("modelMBeanOperations", null);
      }
      desc = (Descriptor) getField.get("modelMBeanDescriptor", null);
      if (desc == null)
         throw new StreamCorruptedException("Null descriptor?");
      this.attributes = (null == attrInfo) ? new MBeanAttributeInfo[0] : (MBeanAttributeInfo[]) attrInfo;
      this.constructors = (null == consInfo) ? new MBeanConstructorInfo[0] : (MBeanConstructorInfo[]) consInfo;
      this.operations = (null == operInfo) ? new MBeanOperationInfo[0] : (MBeanOperationInfo[]) operInfo;
      this.notifications = (null == ntfyInfo) ? new MBeanNotificationInfo[0] : (MBeanNotificationInfo[]) ntfyInfo;
      try
      {
         setMBeanDescriptor(desc);
      }
      catch (MBeanException e)
      {
         throw new StreamCorruptedException(e.toString());
      }
   }

   private void writeObject(ObjectOutputStream oos)
      throws IOException
   {
      ObjectOutputStream.PutField putField = oos.putFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         putField.put("mmbAttributes", attributes);
         putField.put("mmbConstructors", constructors);
         putField.put("mmbNotifications", notifications);
         putField.put("mmbOperations", operations);
         break;
      default:
         putField.put("modelMBeanAttributes", attributes);
         putField.put("modelMBeanConstructors", constructors);
         putField.put("modelMBeanNotifications", notifications);
         putField.put("modelMBeanOperations", operations);
      }
      putField.put("modelMBeanDescriptor", mbeanDescriptor);
      oos.writeFields();
   }
}




