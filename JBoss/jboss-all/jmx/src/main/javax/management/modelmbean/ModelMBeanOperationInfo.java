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

import java.lang.reflect.Method;

import javax.management.MBeanParameterInfo;
import javax.management.MBeanOperationInfo;
import javax.management.DescriptorAccess;
import javax.management.Descriptor;

import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.util.Serialization;

/**
 * Represents Model MBean operation.
 *
 * @see javax.management.modelmbean.ModelMBeanInfo
 * @see javax.management.modelmbean.ModelMBeanInfoSupport
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>.
 * @version $Revision: 1.6.6.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20020320 Juha Lindfors:</b>
 * <ul>
 * <li>toString() implementation</li>
 * </ul>
 *
 * <p><b>20020715 Adrian Brock:</b>
 * <ul>
 * <li> Serialization
 * </ul>
 */
public class ModelMBeanOperationInfo
   extends MBeanOperationInfo
   implements DescriptorAccess, Cloneable
{

   // Attributes ----------------------------------------------------

   /**
    * The descriptor associated with this operation.
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
         serialVersionUID = 9087646304346171239L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("operDescriptor", Descriptor.class)
         };
         break;
      default:
         serialVersionUID = 6532732096650090465L;
         serialPersistentFields = new ObjectStreamField[]
         {
            new ObjectStreamField("operationDescriptor", Descriptor.class)
         };
      }
   }

   // Constructors --------------------------------------------------
   /**
    * Creates a new operation info with a default descriptor.
    *
    * @param   description human readable description string
    * @param   operationMethod a <tt>Method</tt> instance representing the
    *          management operation
    */
   public ModelMBeanOperationInfo(String description, Method operationMethod)
   {
      super(description, operationMethod);
      setDescriptor(createDefaultDescriptor());
   }

   /**
    * Creates a new operation info with a given descriptor. If a <tt>null</tt> or
    * invalid descriptor is passed as a paramter, a default descriptor will be created
    * for the operation.
    *
    * @param   description human readable description string
    * @param   operationMethod a <tt>Method</tt> instance representing the management
    *          operation
    * @param   descriptor a descriptor to associate with this operation
    */
   public ModelMBeanOperationInfo(String description, Method operationMethod, Descriptor descriptor)
   {
      this(description, operationMethod);

      if (descriptor == null || !descriptor.isValid())
         setDescriptor(createDefaultDescriptor());
      else
         setDescriptor(descriptor);
   }

   /**
    * Creates a new operation info with a default descriptor.
    *
    * @param   name name of the operation
    * @param   description human readable description string
    * @param   signature operation signature
    * @param   type a fully qualified name of the operations return type
    * @param   impact operation impact: {@link #INFO INFO}, {@link #ACTION ACTION}, {@link #ACTION_INFO ACTION_INFO}, {@link #UNKNOWN UNKNOWN}
    */
   public ModelMBeanOperationInfo(String name, String description, MBeanParameterInfo[] signature,
                                  String type, int impact)
   {
      super(name, description, signature, type, impact);
      setDescriptor(createDefaultDescriptor());
   }

   /**
    * Creates a new operation info with a given descriptor. If a <tt>null</tt> or invalid
    * descriptor is passed as a parameter, a default descriptor will be created for the operation.
    *
    * @param   name name of the operation
    * @param   description human readable description string
    * @param   signature operation signature
    * @param   type a fully qualified name of the oeprations return type
    * @param   impact operation impact: {@link #INFO INFO}, {@link #ACTION ACTION}, {@link #ACTION_INFO ACTION_INFO}, {@link #UNKNOWN UNKNOWN}
    * @param   descriptor a descriptor to associate with this operation
    */
   public ModelMBeanOperationInfo(String name, String description, MBeanParameterInfo[] signature,
                                  String type, int impact, Descriptor descriptor)
   {
      this(name, description, signature, type, impact);

      if (descriptor == null || !descriptor.isValid())
         setDescriptor(createDefaultDescriptor());
      else
         setDescriptor(descriptor);
   }

   /**
    * Copy constructor.
    *
    * @param   inInfo the operation info to copy
    */
   public ModelMBeanOperationInfo(ModelMBeanOperationInfo info)
   {
      this(info.getName(), info.getDescription(), info.getSignature(),
           info.getReturnType(), info.getImpact(), info.getDescriptor());
   }

   // DescriptorAccess implementation -------------------------------

   /**
    * Returns a copy of the descriptor associated with this operation.
    *
    * @return a copy of this operation's associated descriptor
    */
   public Descriptor getDescriptor()
   {
      return (Descriptor)descriptor.clone();
   }

   /**
    * Replaces the descriptor associated with this operation. If the <tt>inDescriptor</tt>
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
      ModelMBeanOperationInfo clone = (ModelMBeanOperationInfo)super.clone();
      clone.descriptor = (Descriptor)this.descriptor.clone();

      return clone;
   }

   // Object overrides ----------------------------------------------
   /**
    * Returns a string representation of this Model MBean operation info object.
    * The returned string is in the form: <pre>
    *
    *   ModelMBeanOperationInfo[&lt;return type&gt; &lt;operation name&gt;(&lt;signature&gt;),
    *   Impact=ACTION | INFO | ACTION_INFO | UNKNOWN,
    *   Descriptor=(fieldName1=fieldValue1, ... , fieldName&lt;n&gt;=fieldValue&lt;n&gt;)]
    *
    * </pre>
    *
    * @return string representation of this object
    */
   public String toString()
   {
      return "ModelMBeanOperationInfo[" +
             getReturnType() + " " + getName() + getSignatureString() +
             ",Impact=" + getImpactString() +
             ",Descriptor(" + getDescriptor() + ")]";
   }

   // Private -------------------------------------------------------
   private String getSignatureString() 
   {
      StringBuffer sbuf = new StringBuffer(400);
      sbuf.append("(");
      
      MBeanParameterInfo[] sign = getSignature();
      
      if (sign.length > 0)
      {
         for (int i = 0; i < sign.length; ++i)
         {
            sbuf.append(sign[i].getType());
            sbuf.append(" ");
            sbuf.append(sign[i].getName());
         
            sbuf.append(",");
         }
      
         sbuf.delete(sbuf.length() - 1, sbuf.length());
      }
      sbuf.append(")");
      
      return sbuf.toString();
   }
   
   private String getImpactString()
   {
      int impact = getImpact();
      if (impact == MBeanOperationInfo.ACTION)
         return "ACTION";
      else if (impact == MBeanOperationInfo.INFO)
         return "INFO";
      else if (impact == MBeanOperationInfo.ACTION_INFO)
         return "ACTION_INFO";
      else
         return "UNKNOWN";
   }
   
   private Descriptor createDefaultDescriptor()
   {
      DescriptorSupport descr = new DescriptorSupport();
      descr.setField(ModelMBeanConstants.NAME, super.getName());
      descr.setField(ModelMBeanConstants.DESCRIPTOR_TYPE, ModelMBeanConstants.OPERATION_DESCRIPTOR);

      // FIXME: check the spec for all required descriptor fields!

      return descr;
   }

   private void readObject(ObjectInputStream ois)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = ois.readFields();
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         descriptor = (Descriptor) getField.get("operDescriptor", null);
         break;
      default:
         descriptor = (Descriptor) getField.get("operationDescriptor", null);
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
         putField.put("operDescriptor", descriptor);
         break;
      default:
         putField.put("operationDescriptor", descriptor);
      }
      oos.writeFields();
   }
}

