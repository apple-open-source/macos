/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.util.Arrays;

/**
 * Describes a constructor exposed by an MBean
 *
 * This implementation protects its immutability by taking shallow clones of all arrays
 * supplied in constructors and by returning shallow array clones in getXXX() methods.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 *
 * @version $Revision: 1.4.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class MBeanConstructorInfo extends MBeanFeatureInfo
   implements java.io.Serializable, Cloneable
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = 4433990064191844427L;

   // Attributes ----------------------------------------------------
   protected MBeanParameterInfo[] signature = null;

   // Constructors --------------------------------------------------
   public MBeanConstructorInfo(java.lang.String description,
                               java.lang.reflect.Constructor constructor)
   {
      super(constructor.getName(), description);

      Class[] sign = constructor.getParameterTypes();
      signature = new MBeanParameterInfo[sign.length];

      for (int i = 0; i < sign.length; ++i)
      {
         String name = sign[i].getName();
         signature[i] = new MBeanParameterInfo(name, name, "MBean Constructor Parameter.");
      }
   }

   public MBeanConstructorInfo(java.lang.String name,
                               java.lang.String description,
                               MBeanParameterInfo[] signature)
   {
      super(name, description);
      this.signature = (null == signature) ? new MBeanParameterInfo[0] : (MBeanParameterInfo[]) signature.clone();
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
      buffer.append(" signature=").append(Arrays.asList(signature));
      return buffer.toString();
   }

   // Public --------------------------------------------------------
   public MBeanParameterInfo[] getSignature()
   {
      return (MBeanParameterInfo[]) signature.clone();
   }

   // Cloneable implementation --------------------------------------
   public synchronized Object clone() throws CloneNotSupportedException
   {
      MBeanConstructorInfo clone = (MBeanConstructorInfo) super.clone();
      clone.signature = (MBeanParameterInfo[])this.signature.clone();
      
      return clone;
   }
}
