/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

/**
 * Provides shorter method names for working with the {@link PropertyManager}.
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class Property
{
   /**
    * Add a property listener
    *
    * @param listener   Property listener to add
    */
   public static void addListener(PropertyListener listener) {
      PropertyManager.addPropertyListener(listener);
   }

   /**
    * Add an array of property listeners
    *
    * @param listeners     Array of property listeners to add
    */
   public static void addListeners(PropertyListener[] listeners) {
      PropertyManager.addPropertyListeners(listeners);
   }

   /**
    * Remove a property listener
    *
    * @param listener   Property listener to remove
    * @return           True if listener was removed
    */
   public static boolean removeListener(PropertyListener listener) {
      return PropertyManager.removePropertyListener(listener);
   }

   /**
    * Set a property
    *
    * @param name    Property name
    * @param value   Property value
    * @return        Previous property value or null
    */
   public static String set(String name, String value) {
      return PropertyManager.setProperty(name, value);
   }

   /**
    * Remove a property
    *
    * @param name    Property name
    * @return        Removed property value or null
    */
   public static String remove(String name) {
      return PropertyManager.getProperty(name);
   }

   /**
    * Get a property
    *
    * @param name          Property name
    * @param defaultValue  Default property value
    * @return              Property value or default
    */
   public static String get(String name, String defaultValue) {
      return PropertyManager.getProperty(name, defaultValue);
   }

   /**
    * Get a property
    *
    * @param name       Property name
    * @return           Property value or null
    */
   public static String get(String name) {
      return PropertyManager.getProperty(name);
   }

   /**
    * Get an array style property
    * 
    * @param base          Base property name
    * @param defaultValues Default property values
    * @return              Array of property values or default
    */
   public static String[] getArray(String base, String[] defaultValues) {
      return PropertyManager.getArrayProperty(base, defaultValues);
   }

   /**
    * Get an array style property
    *
    * @param name       Property name
    * @return           Array of property values or empty array
    */
   public static String[] getArray(String name) {
      return PropertyManager.getArrayProperty(name);
   }

   /**
    * Check if a property of the given name exists.
    *
    * @param name    Property name
    * @return        True if property exists
    */
   public static boolean exists(String name) {
      return PropertyManager.containsProperty(name);
   }

   /**
    * Get a property group for the given property base
    *
    * @param basename   Base property name
    * @return           Property group
    */
   public static PropertyGroup getGroup(String basename) {
      return PropertyManager.getPropertyGroup(basename);
   }

   /**
    * Get a property group for the given property base at the given index
    *
    * @param basename   Base property name
    * @param index      Array property index
    * @return           Property group
    */
   public static PropertyGroup getGroup(String basename, int index) {
      return PropertyManager.getPropertyGroup(basename, index);
   }
}
