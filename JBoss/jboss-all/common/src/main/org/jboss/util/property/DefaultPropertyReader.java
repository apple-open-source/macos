/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import org.jboss.util.Objects;
import org.jboss.util.CoercionException;

/**
 * Reads properties from files specified via a system property.
 *
 * <p>Unless otherwise specified, propertie filenames will be read from
 *    the <tt>org.jboss.properties</tt> singleton or array property.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class DefaultPropertyReader
   extends FilePropertyReader
{
   //
   // Might want to have a org.jboss.properties.property.name or something
   // property to determine what property name to read from.
   //
   // For now just use 'properties'
   //

   /** Default property name to read filenames from */
   public static final String DEFAULT_PROPERTY_NAME = "properties";

   /**
    * Construct a <tt>DefaultPropertyReader</tt> with a specified property 
    * name.
    *
    * @param name    Property name.
    */
   public DefaultPropertyReader(final String propertyName) {
      super(getFilenames(propertyName));
   }

   /**
    * Construct a <tt>DefaultPropertyReader</tt>.
    */
   public DefaultPropertyReader() {
      this(DEFAULT_PROPERTY_NAME);
   }
   
   /**
    * Get an array of filenames to load.
    *
    * @param propertyName  Property to read filenames from.
    * @return              Array of filenames.
    */
   public static String[] getFilenames(final String propertyName)
      throws PropertyException
   {
      String filenames[];

      // check for singleton property first
      Object filename = PropertyManager.getProperty(propertyName);
      if (filename != null) {
         filenames = new String[] { String.valueOf(filename) };
      }
      else {
         // if no singleton property exists then look for array props
         Object[] values = PropertyManager.getArrayProperty(propertyName);
         try {
            // return coerced string objects
            filenames = (String[])Objects.coerce(values, String[].class);
         }
         catch (CoercionException e) {
            throw new PropertyException(e);
         }
      }

      return filenames;
   }
}
