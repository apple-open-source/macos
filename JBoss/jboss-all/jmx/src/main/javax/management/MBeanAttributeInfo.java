/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.lang.reflect.Method;
import java.io.Serializable;

import org.jboss.mx.util.Serialization;

/**
 * Represents a management attribute in an MBeans' management interface.
 *
 * @see javax.management.MBeanInfo
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.4.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 *   
 */
public class MBeanAttributeInfo extends MBeanFeatureInfo
   implements Serializable, Cloneable
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID;

   static
   {
      switch (Serialization.version)
      {
      case Serialization.V1R0:
         serialVersionUID = 7043855487133450673L;
         break;
      default:
         serialVersionUID = 8644704819898565848L;
      }
   }

   // Attributes ----------------------------------------------------
   
   /**
    * Attribute type string. This is a fully qualified class name of the type.
    */
   private String attributeType        = null;
   
   /**
    * Is attribute readable.
    */
   private boolean isRead = false;
   
   /**
    * Is attribute writable.
    */
   private boolean isWrite = false;
   
   /**
    * Is attribute using the boolean <tt>isAttributeName</tt> naming convention.
    */
   private boolean is       = false;

   
   // Constructors --------------------------------------------------
   
   /**
    * Creates an MBean attribute info object.
    *
    * @param   name name of the attribute
    * @param   type the fully qualified class name of the attribute's type
    * @param   description human readable description string of the attribute
    * @param   isReadable if attribute is readable
    * @param   isWritable if attribute is writable
    * @param   isIs if attribute is using the boolean <tt>isAttributeName</tt> naming convention for its getter method
    */
   public MBeanAttributeInfo(String name, String type, String description,
                             boolean isReadable, boolean isWritable, boolean isIs)
   {
      super(name, description);
   
      this.attributeType = type;
      this.isRead = isReadable;
      this.isWrite = isWritable;
      this.is = isIs;
   }

   /**
    * Creates an MBean attribute info object using the given accessor methods.
    *
    * @param   name        Name of the attribute.
    * @param   description Human readable description string of the attribute's type.
    * @param   getter      The attribute's read accessor. May be <tt>null</tt> if the attribute is write-only.
    * @param   setter      The attribute's write accessor. May be <tt>null</tt> if the attribute is read-only.
    *
    * @throws  IntrospectionException if the accessor methods are not valid for the attribute
    */
   public MBeanAttributeInfo(String name, String description, Method getter, Method setter)
         throws IntrospectionException
   {
      super(name, description);

      if (getter != null)
      {
         // getter must always be no args method, return type cannot be void
         if (getter.getParameterTypes().length != 0)
            throw new IntrospectionException("Expecting getter method to be of the form 'AttributeType getAttributeName()': found getter with " + getter.getParameterTypes().length + " parameters.");
         if (getter.getReturnType() == Void.TYPE)
            throw new IntrospectionException("Expecting getter method to be of the form 'AttributeType getAttributeName()': found getter with void return type.");
            
         this.isRead = true;
         
         if (getter.getName().startsWith("is"))
            this.is = true;
         
         this.attributeType = getter.getReturnType().getName();
      }

      if (setter != null)
      {
         // setter must have one argument, no less, no more. Return type must be void.
         if (setter.getParameterTypes().length != 1)
            throw new IntrospectionException("Expecting the setter method to be of the form 'void setAttributeName(AttributeType value)': found setter with " + setter.getParameterTypes().length + " parameters.");
         if (setter.getReturnType() != Void.TYPE)
            throw new IntrospectionException("Expecting the setter method to be of the form 'void setAttributeName(AttributeType value)': found setter with " + setter.getReturnType() + " return type.");
            
         this.isWrite = true;

         if (attributeType == null)
         {
            try
            {
               attributeType = setter.getParameterTypes() [0].getName();
            }
            catch (ArrayIndexOutOfBoundsException e)
            {
               throw new IntrospectionException("Attribute setter is lacking type: " + name);
            }
         }

         if (!(attributeType.equals(setter.getParameterTypes() [0].getName())))
            throw new IntrospectionException("Attribute type mismatch: " + name);
      }
   }

   
   // Public --------------------------------------------------------

   /**
    * Returns the type string of this attribute.
    *
    * @return fully qualified class name of the attribute's type
    */
   public String getType()
   {
      return attributeType;
   }
   
   /**
    * If the attribute is readable.
    *
    * @return true if attribute is readable; false otherwise
    */
   public boolean isReadable()
   {
      return isRead;
   }

   /**
    * If the attribute is writable.
    *
    * @return true if attribute is writable; false otherwise
    */
   public boolean isWritable()
   {
      return isWrite;
   }

   /**
    * If the attribute is using the boolean <tt>isAttributeName</tt> naming convention
    * for its read accessor.
    *
    * @param   true if using <tt>isAttributeName</tt> getter; false otherwise
    */
   public boolean isIs()
   {
      return is;
   }

   /**
    * @returns a human readable string
    */
   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(getClass().getName()).append(":");
      buffer.append(" name=").append(getName());
      buffer.append(" description=").append(getDescription());
      buffer.append(" type=").append(getType());
      buffer.append(" Readable=").append(isReadable());
      buffer.append(" Writable=").append(isWritable());
      buffer.append(" isIs=").append(isIs());
      return buffer.toString();
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
      MBeanAttributeInfo clone = (MBeanAttributeInfo)super.clone();
      clone.attributeType = this.attributeType;
      clone.isRead        = this.isRead;
      clone.isWrite       = this.isWrite;
      clone.is            = this.is;

      return clone;
   }

}


