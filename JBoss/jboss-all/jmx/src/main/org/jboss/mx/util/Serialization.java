/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.util;

/**
 * Serialization Helper.<p>
 *
 * Contains static constants and attributes to help is serialization
 * versioning.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1.2.2 $
 */
public class Serialization
{
   // Static --------------------------------------------------------

   /**
    * The latest version of serialization
    */
   public static final int LATEST = 0;

   /**
    * The serialization for the 1.0 specified in the spec 1.1
    */
   public static final int V1R0 = 10;

   /**
    * The serialization version to use
    */
   public static int version = LATEST;

   /**
    * Determine the serialization version
    */
   static
   {
      try
      {
         String property = System.getProperty("jmx.serial.form");
         if (property != null && property.equals("1.0"))
            version = V1R0;
      }
      catch (java.security.AccessControlException appletSupport)
      {
      }
   }
}

