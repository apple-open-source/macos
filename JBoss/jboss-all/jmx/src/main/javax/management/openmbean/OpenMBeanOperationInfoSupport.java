/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.io.Serializable;

import java.util.Arrays;

import javax.management.MBeanOperationInfo;
import javax.management.MBeanParameterInfo;

/**
 * OpenMBeanOperationInfo implementation
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public class OpenMBeanOperationInfoSupport
   extends MBeanOperationInfo
   implements OpenMBeanOperationInfo, Serializable
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = 4996859732565369366L;

   // Attributes ----------------------------------------------------

   /**
    * The open type of the return value
    */
   private OpenType returnOpenType;

   private transient int cachedHashCode;

   private transient String cachedToString;

   // Static --------------------------------------------------------

   private static MBeanParameterInfo[] convertArray(OpenMBeanParameterInfo[] array)
   {
      if (array == null)
         return null;
      MBeanParameterInfo[] result = new MBeanParameterInfo[array.length];
      System.arraycopy(array, 0, result, 0, array.length);
      return result;
   }

   // Constructors --------------------------------------------------

   /**
    * Contruct an OpenMBeanOperationInfoSupport<p>
    *
    * @param name cannot be null or empty
    * @param description cannot be null or empty
    * @param signature the parameters of the operation
    * @param returnOpenType the open type of the return value
    * @param impact the impact of the operation
    * @exception IllegalArgumentException when one of the above
    *            constraints is not satisfied
    */
   public OpenMBeanOperationInfoSupport(String name, String description,
                                        OpenMBeanParameterInfo[] signature,
                                        OpenType returnOpenType, int impact)
   {
      super(name, description, convertArray(signature), 
            returnOpenType == null ? null : returnOpenType.getClassName(), impact);
      if (name == null)
         throw new IllegalArgumentException("null name");
      if (name.trim().length() == 0)
         throw new IllegalArgumentException("empty name");
      if (description == null)
         throw new IllegalArgumentException("null description");
      if (description.trim().length() == 0)
         throw new IllegalArgumentException("empty description");
      if (returnOpenType == null)
         throw new IllegalArgumentException("null return open type");
      if (impact != ACTION && impact != ACTION_INFO && impact != INFO)
         throw new IllegalArgumentException("Invalid action");

      this.returnOpenType = returnOpenType;
   }

   // Public --------------------------------------------------------

   public OpenType getReturnOpenType()
   {
      return returnOpenType;
   }

   // OpenMBeanOperationInfo Implementation -------------------------

   // Object Overrides ----------------------------------------------

   public boolean equals(Object obj)
   {
      if (this == obj)
         return true;
      if (obj == null || !(obj instanceof OpenMBeanOperationInfoSupport))
         return false;
      OpenMBeanOperationInfo other = (OpenMBeanOperationInfo) obj;

      if (getName().equals(other.getName()) == false)
         return false;

      if (getReturnOpenType().equals(other.getReturnOpenType()) == false)
         return false;

      if (Arrays.asList(getSignature()).equals(Arrays.asList(other.getSignature())) == false)
         return false;

      if (getImpact() != other.getImpact())
         return false;

      return true;
   }

   public int hashCode()
   {
      if (cachedHashCode != 0)
        return cachedHashCode;
      cachedHashCode = getName().hashCode();
      cachedHashCode += getReturnOpenType().hashCode();
      cachedHashCode += Arrays.asList(getSignature()).hashCode();
      cachedHashCode += getImpact();
      return cachedHashCode;
   }

   public String toString()
   {
      if (cachedToString != null)
         return cachedToString;
      StringBuffer buffer = new StringBuffer(getClass().getName());
      buffer.append(": name=");
      buffer.append(getName());
      buffer.append(", returnOpenType=");
      buffer.append(getReturnOpenType());
      buffer.append(", signature=");
      buffer.append(Arrays.asList(getSignature()));
      buffer.append(", impact=");
      switch (getImpact())
      {
      case ACTION:      buffer.append("ACTION");      break;
      case ACTION_INFO: buffer.append("ACTION_INFO"); break;
      case INFO:        buffer.append("INFO");        break;
      default:          buffer.append("UNKNOWN");
      }
      cachedToString = buffer.toString();
      return cachedToString;
   }

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner Classes -------------------------------------------------
}
