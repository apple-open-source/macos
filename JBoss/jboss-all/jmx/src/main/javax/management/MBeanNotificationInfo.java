/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.util.Arrays;

/**
 * Describes a notification emitted by an MBean
 *
 * This implementation protects its immutability by taking shallow clones of all arrays
 * supplied in constructors and by returning shallow array clones in getXXX() methods.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author  <a href="mailto:trevor@protocool.com">Trevor Squires</a>.
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 *
 * @version $Revision: 1.2.8.1 $
 *
 * <p><b>Revisions:</b>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 */
public class MBeanNotificationInfo extends MBeanFeatureInfo
   implements Cloneable, java.io.Serializable
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = -3888371564530107064L;

   // Attributes ----------------------------------------------------
   protected String[] types = null;

   // Constructors --------------------------------------------------
   public MBeanNotificationInfo(String[] notifsType,
                                String name,
                                String description)
   {
      super(name, description);
      this.types = (null == notifsType) ? new String[0] : (String[]) notifsType.clone();
   }

   // Public -------------------------------------------------------
   public String[] getNotifTypes()
   {
      return (String[]) types.clone();
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
      buffer.append(" types=").append(Arrays.asList(types));
      return buffer.toString();
   }

   // CLoneable implementation -------------------------------------
   public Object clone() throws CloneNotSupportedException
   {
      MBeanNotificationInfo clone = (MBeanNotificationInfo) super.clone();
      clone.types = getNotifTypes();

      return clone;
   }
}
