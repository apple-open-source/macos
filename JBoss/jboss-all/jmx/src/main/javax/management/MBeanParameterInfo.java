/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Describes an argument of an operation exposed by an MBean
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
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
public class MBeanParameterInfo extends MBeanFeatureInfo
   implements java.io.Serializable, Cloneable
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = 7432616882776782338L;

   // Attributes ----------------------------------------------------
   protected String type = null;

   // Constructors --------------------------------------------------
   public MBeanParameterInfo(java.lang.String name,
                             java.lang.String type,
                             java.lang.String description)
   {
      super(name, description);
      this.type = type;
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
      return buffer.toString();
   }

   // Public --------------------------------------------------------
   public java.lang.String getType()
   {
      return type;
   }

   // Cloneable implementation --------------------------------------
   public java.lang.Object clone() throws CloneNotSupportedException
   {
      MBeanParameterInfo clone = (MBeanParameterInfo) super.clone();
      clone.type = getType();
      return clone;
   }
}
