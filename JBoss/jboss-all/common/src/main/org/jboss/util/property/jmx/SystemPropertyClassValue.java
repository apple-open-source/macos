/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property.jmx;

import org.jboss.logging.Logger;

/**
 * A helper for setting system properties based on class availablity.<p>
 *
 * It has a static method and an MBean wrapper for dynamic configuration.<p>
 *
 * The class is first checked for availablity before setting the system
 * property.

 * @jmx.mbean
 *
 * @version <tt>$Revision: 1.1.4.3 $</tt>
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 */
public class SystemPropertyClassValue
   implements SystemPropertyClassValueMBean
{
   public static final Logger log = Logger.getLogger(SystemPropertyClassValue.class);

   /** Property name. */
   protected String property;

   /** Class Name. */
   protected String className;

   /**
    * Constructor.
    */
   public SystemPropertyClassValue()
   {
   }

   /**
    * The system property value
    *
    * @jmx.managed-attribute
    */
   public String getProperty()
   {
      return property;
   }

   /**
    * The system property value
    *
    * @jmx.managed-attribute
    */
   public void setProperty(String property)
   {
      this.property = property;
   }

   /**
    * The class name to use a value for the system property
    * when it is available
    *
    * @jmx.managed-attribute
    */
   public String getClassName()
   {
      return className;
   }

   /**
    * The class name to use a value for the system property
    * when it is available
    *
    * @jmx.managed-attribute
    */
   public void setClassName(String className)
   {
      this.className = className;
   }

   /**
    * JBoss lifecycle
    *
    * @jmx.managed-operation
    */
   public void create()
   {
      Throwable error = setSystemPropertyClassValue(property, className);
      if (error != null)
         log.trace("Error loading class " + className + " property " + property + " not set.", error);
   }

   /**
    * Sets the system property to a class when the class is available.
    *
    * @param property the property to set
    * @param className the class to set as the properties value
    * @return any error loading the class
    * @exception IllegalArgumentException for a null or empty parameter
    */
   public static Throwable setSystemPropertyClassValue(String property, String className)
   {
      // Validation
      if (property == null || property.trim().length() == 0)
         throw new IllegalArgumentException("Null or empty property");
      if (className == null || className.trim().length() == 0)
         throw new IllegalArgumentException("Null or empty class name");

      // Is the class available?
      try
      {
         Thread.currentThread().getContextClassLoader().loadClass(className);
      }
      catch (Throwable problem)
      {
         return problem;
      }

      // The class is there, set the property.
      System.setProperty(property, className);
      return null;
   }
}
