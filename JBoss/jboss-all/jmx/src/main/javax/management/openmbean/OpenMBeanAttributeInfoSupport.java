/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.io.Serializable;

import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;

import javax.management.MBeanAttributeInfo;

/**
 * OpenMBeanAttributeInfo implementation
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public class OpenMBeanAttributeInfoSupport
   extends MBeanAttributeInfo
   implements OpenMBeanAttributeInfo, Serializable
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = -4867215622149721849L;

   // Attributes ----------------------------------------------------

   /**
    * The OpenType of this attribute
    */
   private OpenType openType;

   /**
    * The default value of this attribute
    */
   private Object defaultValue;

   /**
    * The legal values of this attribute
    */
   private Set legalValues;

   /**
    * The minimum value of this attribute
    */
   private Comparable minValue;

   /**
    * The maximum value of this attribute
    */
   private Comparable maxValue;

   private transient int cachedHashCode;

   private transient String cachedToString;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Contruct an OpenMBeanAttributeInfoSupport<p>
    *
    * @param name cannot be null or empty
    * @param description cannot be null or empty
    * @param openType cannot be null
    * @param isReadable true when the getter is exposed for management
    * @param isWritable true when the setter is exposed for management
    * @param isIs true when the getter is of the form isXXX
    * @exception IllegalArgumentException when one of the above
    *            constraints is not satisfied
    */
   public OpenMBeanAttributeInfoSupport(String name, String description,
                                        OpenType openType, boolean isReadable,
                                        boolean isWritable, boolean isIs)
   {
      super(name, openType == null ? null : openType.getClassName(),
            description, isReadable, isWritable, isIs);
      try
      {
         init(name, description, openType, isReadable, isWritable, isIs, null,
              null, null, null);
      }
      catch (OpenDataException notRelevent)
      {
      }
   }

   /**
    * Contruct an OpenMBeanAttributeInfoSupport<p>
    *
    * @param name cannot be null or empty
    * @param description cannot be null or empty
    * @param openType cannot be null
    * @param isReadable true when the getter is exposed for management
    * @param isWritable true when the setter is exposed for management
    * @param isIs true when the getter is of the form isXXX
    * @param defaultValue the default value
    * @exception IllegalArgumentException when one of the above
    *            constraints is not satisfied
    * @exception OpenDataException when default value is not correct for
    *            the open type or cannot specify a default value for
    *            ArrayType and TabularType
    */
   public OpenMBeanAttributeInfoSupport(String name, String description,
                                        OpenType openType, boolean isReadable,
                                        boolean isWritable, boolean isIs,
                                        Object defaultValue)
      throws OpenDataException
   {
      super(name, openType == null ? null : openType.getClassName(),
            description, isReadable, isWritable, isIs);
      init(name, description, openType, isReadable, isWritable, isIs, defaultValue,
           null, null, null);
   }

   /**
    * Contruct an OpenMBeanAttributeInfoSupport<p>
    *
    * @param name cannot be null or empty
    * @param description cannot be null or empty
    * @param openType cannot be null
    * @param isReadable true when the getter is exposed for management
    * @param isWritable true when the setter is exposed for management
    * @param isIs true when the getter is of the form isXXX
    * @param defaultValue the default value
    * @param legalValues an array of legal values
    * @exception IllegalArgumentException when one of the above
    *            constraints is not satisfied
    */
   public OpenMBeanAttributeInfoSupport(String name, String description,
                                        OpenType openType, boolean isReadable,
                                        boolean isWritable, boolean isIs,
                                        Object defaultValue, Object[] legalValues)
      throws OpenDataException
   {
      super(name, openType == null ? null : openType.getClassName(),
            description, isReadable, isWritable, isIs);
      init(name, description, openType, isReadable, isWritable, isIs, defaultValue,
           legalValues, null, null);
   }

   /**
    * Contruct an OpenMBeanAttributeInfoSupport<p>
    *
    * @param name cannot be null or empty
    * @param description cannot be null or empty
    * @param openType cannot be null
    * @param isReadable true when the getter is exposed for management
    * @param isWritable true when the setter is exposed for management
    * @param isIs true when the getter is of the form isXXX
    * @param defaultValue the default value
    * @param minValue the minimum value
    * @param maxValue the maximum value
    * @exception IllegalArgumentException when one of the above
    *            constraints is not satisfied
    */
   public OpenMBeanAttributeInfoSupport(String name, String description,
                                        OpenType openType, boolean isReadable,
                                        boolean isWritable, boolean isIs,
                                        Object defaultValue, Comparable minValue,
                                        Comparable maxValue)
      throws OpenDataException
   {
      super(name, openType == null ? null : openType.getClassName(),
            description, isReadable, isWritable, isIs);
      init(name, description, openType, isReadable, isWritable, isIs, defaultValue,
           null, minValue, maxValue);
   }

   // Public --------------------------------------------------------

   // OpenMBeanAttributeInfo Implementation -------------------------

   public Object getDefaultValue()
   {
      return defaultValue;
   }

   public Set getLegalValues()
   {
      return legalValues;
   }

   public Comparable getMinValue()
   {
      return minValue;
   }

   public Comparable getMaxValue()
   {
      return maxValue;
   }

   public OpenType getOpenType()
   {
      return openType;
   }

   public boolean hasDefaultValue()
   {
      return (defaultValue != null);
   }

   public boolean hasLegalValues()
   {
      return (legalValues != null);
   }

   public boolean hasMinValue()
   {
      return (minValue != null);
   }

   public boolean hasMaxValue()
   {
      return (maxValue != null);
   }

   public boolean isValue(Object obj)
   {
      if (openType.isValue(obj) == false)
         return false;
      if (minValue != null && minValue.compareTo(obj) > 0)
         return false;
      if (maxValue != null && maxValue.compareTo(obj) < 0)
         return false;
      if (legalValues != null && legalValues.contains(obj) == false)
         return false;
      return true;
   }

   // Object Overrides ----------------------------------------------

   public boolean equals(Object obj)
   {
      if (this == obj)
         return true;
      if (obj == null || !(obj instanceof OpenMBeanAttributeInfo))
         return false;
      OpenMBeanAttributeInfo other = (OpenMBeanAttributeInfo) obj;

      if (this.getName().equals(other.getName()) == false)
         return false;

      if (this.getOpenType().equals(other.getOpenType()) == false)
         return false;

      if (isReadable() != other.isReadable())
         return false;

      if (isWritable() != other.isWritable())
         return false;

      if (isIs() != other.isIs())
         return false;

      if (hasDefaultValue() == false && other.hasDefaultValue() == true)
         return false;
      if (hasDefaultValue() == true && this.getDefaultValue().equals(other.getDefaultValue()) == false)
         return false;

      if (hasMinValue() == false && other.hasMinValue() == true)
         return false;
      if (hasMinValue() == true && this.getMinValue().equals(other.getMinValue()) == false )
         return false;

      if (hasMaxValue() == false && other.hasMaxValue() == true)
         return false;
      if (hasMaxValue() == true && this.getMaxValue().equals(other.getMaxValue()) == false)
         return false;

      if (hasLegalValues() == false && other.hasLegalValues() == true)
         return false;
      if (hasLegalValues() == true)
      {
         Set otherLegal = other.getLegalValues();
         if (otherLegal == null)
            return false;
         Set thisLegal = this.getLegalValues();
         if (thisLegal.size() != otherLegal.size()) 
            return false;
         if (thisLegal.containsAll(otherLegal) == false)
            return false;
      }
      return true;
   }

   public int hashCode()
   {
      if (cachedHashCode != 0)
        return cachedHashCode;
      cachedHashCode = getName().hashCode();
      cachedHashCode += getOpenType().hashCode();
      if (defaultValue != null)
         cachedHashCode += getDefaultValue().hashCode();
      if (minValue != null)
         cachedHashCode += getMinValue().hashCode();
      if (maxValue != null)
         cachedHashCode += getMaxValue().hashCode();
      if (legalValues != null)
         cachedHashCode += getLegalValues().hashCode();
      return cachedHashCode;
   }

   public String toString()
   {
      if (cachedToString != null)
         return cachedToString;
      StringBuffer buffer = new StringBuffer(getClass().getName());
      buffer.append(": name=");
      buffer.append(getName());
      buffer.append(", openType=");
      buffer.append(getOpenType());
      buffer.append(", isWritable=");
      buffer.append(isWritable());
      buffer.append(", isReadable=");
      buffer.append(isReadable());
      buffer.append(", isIs=");
      buffer.append(isIs());
      buffer.append(", defaultValue=");
      buffer.append(getDefaultValue());
      buffer.append(", minValue=");
      buffer.append(getMinValue());
      buffer.append(", maxValue=");
      buffer.append(getMaxValue());
      buffer.append(", legalValues=");
      buffer.append(getLegalValues());
      cachedToString = buffer.toString();
      return cachedToString;
   }

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   /**
    * Initialise an OpenMBeanAttributeInfoSupport<p>
    *
    * WARNING: For the MBeanAttributeInfo only validation is performed
    *
    * @param name cannot be null or empty
    * @param description cannot be null or empty
    * @param openType cannot be null
    * @param isReadable true when the getter is exposed for management
    * @param isWritable true when the setter is exposed for management
    * @param isIs true when the getter is of the form isXXX
    * @param defaultValue the default value
    * @param minValue the minimum value
    * @param maxValue the maximum value
    * @exception IllegalArgumentException when one of the above
    *            constraints is not satisfied
    */
   private void init(String name, String Description,
                     OpenType openType, boolean isReadable,
                     boolean isWritable, boolean isIs,
                     Object defaultValue, Object[] legalValues,
                     Comparable minValue, Comparable maxValue)
      throws OpenDataException
   {
      if (name == null || name.trim().length() == 0)
         throw new IllegalArgumentException("null or empty name");

      if (description == null || description.trim().length() == 0)
         throw new IllegalArgumentException("null or empty description");

      if (openType == null)
         throw new IllegalArgumentException("null open type");
      this.openType = openType;

      if (defaultValue != null && 
         (openType instanceof ArrayType || openType instanceof TabularType))
         throw new OpenDataException("default value is not supported for "
                                     + openType.getClass().getName());
      if (defaultValue != null && openType.isValue(defaultValue) == false)
         throw new OpenDataException("default value is not valid for "
                                     + openType.getClass().getName());

      if (legalValues != null && legalValues.length != 0)
      {
         if (openType instanceof ArrayType || openType instanceof TabularType)
            throw new OpenDataException("legal values are not supported for "
                                               + openType.getClass().getName());
         HashSet legals = new HashSet(legalValues.length);
         for (int i = 0; i < legalValues.length; i++)
         {
            if (openType.isValue(legalValues[i]) == false)
            throw new OpenDataException("legal value " + legalValues[i] +  " at index " + i
                                        + " is not valid for " + openType.getClass().getName());
            legals.add(legalValues[i]);
         }
         if (defaultValue != null && legals.contains(defaultValue) == false)
               throw new OpenDataException("default value is not a legal value");
         this.legalValues = Collections.unmodifiableSet(legals);
      }

      if (minValue != null && openType.isValue(minValue) == false)
         throw new OpenDataException("minimum value is not valid for "
                                            + openType.getClass().getName());
      if (defaultValue != null && minValue != null && minValue.compareTo(defaultValue) > 0)
         throw new OpenDataException("the default value is less than the minimum value ");

      if (maxValue != null && openType.isValue(maxValue) == false)
         throw new OpenDataException("maximum value is not valid for "
                                            + openType.getClass().getName());
      if (defaultValue != null && maxValue != null && maxValue.compareTo(defaultValue) < 0)
         throw new OpenDataException("the default value is greater than the maximum value ");

      if (minValue != null && maxValue != null && minValue.compareTo(maxValue) > 0)
         throw new OpenDataException("the minimum value is greater than the maximum value ");

      this.defaultValue = defaultValue;
      this.minValue = minValue;
      this.maxValue = maxValue;
   }

   // Inner Classes -------------------------------------------------
}
