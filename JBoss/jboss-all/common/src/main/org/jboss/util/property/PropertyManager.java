/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import java.util.Map;
import java.util.Iterator;

import java.io.IOException;

import org.jboss.util.ThrowableHandler;
   
/**
 * A more robust replacement of <tt>java.lang.System</tt> for property
 * access.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public final class PropertyManager
{
   /** Property reader list property name */
   public static final String READER_PROPERTY_NAME = "org.jboss.util.property.reader";

   /** Token which specifies the default property reader */
   public static final String DEFAULT_PROPERTY_READER_TOKEN = "DEFAULT";

   /** The default property reader name array */
   private static final String[] DEFAULT_PROPERTY_READERS = { 
      DEFAULT_PROPERTY_READER_TOKEN
   };

   /** Default property container */
   private static PropertyMap props;

   /**
    * Do not allow instantiation of this class.
    */
   private PropertyManager() {}
   

   /////////////////////////////////////////////////////////////////////////
   //                    Property System Initialization                   //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Initialize the property system.
    */
   static {
      // construct default property container and initialze from system props
      props = new PropertyMap();
      props.putAll(System.getProperties());

      // replace system props to enable notifications via System.setProperty()
      System.setProperties(props);

      // load properties from initial property readers
      String[] readerNames = getArrayProperty(READER_PROPERTY_NAME,
                                              DEFAULT_PROPERTY_READERS);

      // construct each source and read its properties
      for (int i=0; i<readerNames.length; i++) {
         try {
            if (readerNames[i].equals(DEFAULT_PROPERTY_READER_TOKEN)) {
               load(new DefaultPropertyReader());
            }
            else {
               load(readerNames[i]);
            }
         }
         catch (IOException e) {
            ThrowableHandler.add(e);
         }
      }
   }

   /////////////////////////////////////////////////////////////////////////
   //                        Access to Default Map                        //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Get the default <tt>PropertyMap</tt>.
    *
    * @return  Default <tt>PropertyMap</tt>.
    */
   public static PropertyMap getDefaultPropertyMap() {
      return props;
   }

   /////////////////////////////////////////////////////////////////////////
   //               Static Accessors to Default Property Map              //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Add a property listener.
    *
    * @param listener   Property listener to add.
    */
   public static void addPropertyListener(final PropertyListener listener) {
      props.addPropertyListener(listener);
   }

   /**
    * Add an array of property listeners.
    *
    * @param listeners     Array of property listeners to add.
    */
   public static void addPropertyListeners(final PropertyListener[] listeners)
   {
      props.addPropertyListeners(listeners);
   }

   /**
    * Remove a property listener.
    *
    * @param listener   Property listener to remove.
    * @return           True if listener was removed.
    */
   public static boolean removePropertyListener(final PropertyListener listener)
   {
      return props.removePropertyListener(listener);
   }

   /**
    * Load properties from a map.
    *
    * @param prefix  Prefix to append to all map keys (or <tt>null</tt>).
    * @param map     Map containing properties to load.
    */
   public static void load(final String prefix,
                           final Map map)
      throws PropertyException
   {
      props.load(prefix, map);
   }

   /**
    * Load properties from a map.
    *
    * @param map  Map containing properties to load.
    */
   public static void load(final Map map)
      throws PropertyException, IOException
   {
      props.load(map);
   }

   /**
    * Load properties from a <tt>PropertyReader</tt>.
    *
    * @param reader  <tt>PropertyReader</tt> to read properties from.
    */
   public static void load(final PropertyReader reader)
      throws PropertyException, IOException
   {
      props.load(reader);
   }

   /**
    * Load properties from a <tt>PropertyReader</tt> specifed by the 
    * given class name.
    *
    * @param classname     Class name of a <tt>PropertyReader</tt> to 
    *                      read from.
    */
   public static void load(final String classname)
      throws PropertyException, IOException
   {
      props.load(classname);
   }

   /**
    * Set a property.
    *
    * @param name    Property name.
    * @param value   Property value.
    * @return        Previous property value or <tt>null</tt>.
    */
   public static String setProperty(final String name, final String value) {
      return (String)props.setProperty(name, value);
   }

   /**
    * Remove a property.
    *
    * @param name    Property name.
    * @return        Removed property value or <tt>null</tt>.
    */
   public static String removeProperty(final String name) {
      return props.removeProperty(name);
   }

   /**
    * Get a property.
    *
    * @param name          Property name.
    * @param defaultValue  Default property value.
    * @return              Property value or default.
    */
   public static String getProperty(final String name,
                                    final String defaultValue)
   {
      return props.getProperty(name, defaultValue);
   }

   /**
    * Get a property.
    *
    * @param name       Property name.
    * @return           Property value or <tt>null</tt>.
    */
   public static String getProperty(final String name) {
      return props.getProperty(name);
   }

   /**
    * Get an array style property.
    * 
    * @param base             Base property name.
    * @param defaultValues    Default property values.
    * @return                 Array of property values or default.
    */
   public static String[] getArrayProperty(final String base,
                                           final String[] defaultValues)
   {
      return props.getArrayProperty(base, defaultValues);
   }

   /**
    * Get an array style property.
    *
    * @param name       Property name.
    * @return           Array of property values or empty array.
    */
   public static String[] getArrayProperty(final String name) {
      return props.getArrayProperty(name);
   }

   /**
    * Return an iterator over all contained property names.
    *
    * @return     Property name iterator.
    */
   public static Iterator names() {
      return props.names();
   }

   /**
    * Check if this map contains a given property.
    *
    * @param name    Property name.
    * @return        True if contains property.
    */
   public static boolean containsProperty(final String name) {
      return props.containsProperty(name);
   }

   /**
    * Get a property group for the given property base.
    *
    * @param basename   Base property name.
    * @return           Property group.
    */
   public static PropertyGroup getPropertyGroup(final String basename) {
      return props.getPropertyGroup(basename);
   }

   /**
    * Get a property group for the given property base at the given index.
    *
    * @param basename   Base property name.
    * @param index      Array property index.
    * @return           Property group.
    */
   public static PropertyGroup getPropertyGroup(final String basename,
                                                final int index)
   {
      return props.getPropertyGroup(basename, index);
   }
}  
