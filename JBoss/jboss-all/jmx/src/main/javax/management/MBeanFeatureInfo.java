/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.io.Serializable;

/**
 * General information for MBean descriptor objects.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 *
 * <p><b>Revisions:</b>
 * <p><b>20020711 Adrian Brock:</b>
 * <ul>
 * <li> Serialization </li>
 * </ul>
 *
 * @version $Revision: 1.3.8.1 $
 */
public class MBeanFeatureInfo 
   implements Serializable
{
   // Constants -----------------------------------------------------

   private static final long serialVersionUID = 3952882688968447265L;

   // Attributes ----------------------------------------------------
   
   /**
    * Name of the MBean feature.
    */
   protected String name = null;
   
   /**
    * Human readable description string of the MBean feature.
    */
   protected String description = null;

   // Constructors --------------------------------------------------
   
   /**
    * Constructs an MBean feature info object.
    *
    * @param   name name of the MBean feature
    * @param   description human readable description string of the feature
    */
   public MBeanFeatureInfo(String name, String description)
   {
      this.name = name;
      this.description = description;
   }

   // Public --------------------------------------------------------
   
   /**
    * Returns the name of the MBean feature.
    *
    * @return  name string
    */
   public String getName()
   {
      return name;
   }

   /** 
    * Returns the description of the MBean feature.
    *
    * @return  a human readable description string
    */
   public String getDescription()
   {
      return description;
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
      return buffer.toString();
   }
}
