/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.List;
import java.util.LinkedList;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.HashSet;
import java.util.Collections;
import java.util.Properties;
import javax.naming.Context;
import javax.naming.ldap.LdapContext;

import org.jboss.util.NullArgumentException;

/**
 * A replacement for the standard <code>java.util.Properties</code>
 * class which adds, among others, property event capabilities.
 *
 * @todo consider moving the JNDI property handling to a InitialContextFactoryBuilder
 * 
 * @version <tt>$Revision: 1.2.2.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public class PropertyMap
   extends Properties
{
   /** Property name separator */
   public static final String PROPERTY_NAME_SEPARATOR = ".";

   /** Empty array property */
   public static final String[] EMPTY_ARRAY_PROPERTY = new String[0];

   /** Property listener list */
   protected transient List unboundListeners;

   /** Bound property name -> listener list map */
   protected transient Map boundListeners;

   /**
    * This map avoids heavy contention for the properties that JNDI looks
    * up everytime a new InitialContext instance is created. Once the container is
    * up and running getProperty calls other than for the JNDI property are very rare,
    * so the double lookup is not much of a performance problem.
    * If at all possible, this class should be read-only and use no locks at all.
    */
   private transient Map jndiMap;
   private final static Object NULL_VALUE = new Object();

   /**
    * Construct a PropertyMap with default properties.
    *
    * @param defaults   Default properties.
    */
   public PropertyMap(Properties defaults)
   {
      super(defaults);
      init();
   }

   /**
    * Construct a PropertyMap.
    */
   public PropertyMap()
   {
      this(null);
   }

   /** Initialized listener lists and the JNDI properties cache map
    */
   private void init()
   {
      unboundListeners = Collections.synchronizedList(new ArrayList());
      boundListeners = Collections.synchronizedMap(new HashMap());
      Object value;
      jndiMap = new HashMap();

      value = System.getProperty(Context.PROVIDER_URL);
      if (value == null) value = NULL_VALUE;
      jndiMap.put(Context.PROVIDER_URL, value);

      value = System.getProperty(Context.INITIAL_CONTEXT_FACTORY);
      if (value == null) value = NULL_VALUE;
      jndiMap.put(Context.INITIAL_CONTEXT_FACTORY, value);

      value = System.getProperty(Context.OBJECT_FACTORIES);
      if (value == null) value = NULL_VALUE;
      jndiMap.put(Context.OBJECT_FACTORIES, value);

      value = System.getProperty(Context.URL_PKG_PREFIXES);
      if (value == null) value = NULL_VALUE;
      jndiMap.put(Context.URL_PKG_PREFIXES, value);

      value = System.getProperty(Context.STATE_FACTORIES);
      if (value == null) value = NULL_VALUE;
      jndiMap.put(Context.STATE_FACTORIES, value);

      value = System.getProperty(Context.DNS_URL);
      if (value == null) value = NULL_VALUE;
      jndiMap.put(Context.DNS_URL, value);

      value = System.getProperty(LdapContext.CONTROL_FACTORIES);
      if (value == null) value = NULL_VALUE;
      jndiMap.put(LdapContext.CONTROL_FACTORIES, value);
   }

   /** Called by setProperty to update the jndiMap cache values.
    * @param name the property name
    * @param value the property value
    */ 
   private void updateJndiCache(String name, String value)
   {
      if( name == null )
         return;

      boolean isJndiProperty = name.equals(Context.PROVIDER_URL)
         || name.equals(Context.INITIAL_CONTEXT_FACTORY)
         || name.equals(Context.OBJECT_FACTORIES)
         || name.equals(Context.URL_PKG_PREFIXES)
         || name.equals(Context.STATE_FACTORIES)
         || name.equals(Context.DNS_URL)
         || name.equals(LdapContext.CONTROL_FACTORIES)
      ;
      if( isJndiProperty == true )
         jndiMap.put(name, value);
   }

   /////////////////////////////////////////////////////////////////////////
   //                     Properties Override Methods                     //
   /////////////////////////////////////////////////////////////////////////


   /**
    * Set a property.
    *
    * @param name    Property name.
    * @param value   Property value.
    * @return        Previous property value or null.
    */
   public Object put(Object name, Object value)
   {
      if (name == null)
         throw new NullArgumentException("name");
      // value can be null

      // check if this is a new addition or not prior to updating the hash
      boolean add = !containsKey(name);
      Object prev = super.put(name, value);

      PropertyEvent event =
         new PropertyEvent(this, String.valueOf(name), String.valueOf(value));
      
      // fire propertyAdded or propertyChanged
      if (add)
      {
         firePropertyAdded(event);
      }
      else
      {
         firePropertyChanged(event);
      }

      return prev;
   }

   /**
    * Remove a property.
    *
    * @param name    Property name.
    * @return        Removed property value.
    */
   public Object remove(Object name)
   {
      if (name == null)
         throw new NullArgumentException("name");

      // check if there is a property with this name
      boolean contains = containsKey(name);
      String value = null;

      if (contains)
      {
         value = (String) super.remove(name);
         if (defaults != null)
         {
            Object obj = defaults.remove(name);
            if (value == null)
            {
               value = (String) obj;
            }
         }
         // Remove any JNDI property value
         jndiMap.remove(name);

         PropertyEvent event = new PropertyEvent(this, (String) name, value);
         firePropertyRemoved(event);
      }

      return value;
   }

   /**
    * Returns a set of keys for all entries in this group and optionally
    * all of the keys in the defaults map.
    */
   public Set keySet(final boolean includeDefaults)
   {
      if (includeDefaults)
      {
         Set set = new HashSet();
         set.addAll(defaults.keySet());
         set.addAll(super.keySet());
         return Collections.synchronizedSet(set);
      }

      return super.keySet();
   }

   /**
    * Returns a set of entrys for all entries in this group and optionally
    * all of the entrys in the defaults map.
    */
   public Set entrySet(final boolean includeDefaults)
   {
      if (includeDefaults)
      {
         Set set = new HashSet();
         set.addAll(defaults.entrySet());
         set.addAll(super.entrySet());
         return Collections.synchronizedSet(set);
      }

      return super.entrySet();
   }   
   

   /////////////////////////////////////////////////////////////////////////
   //                      Property Listener Methods                      //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Add a property listener.
    *
    * @param listener   Property listener to add.
    */
   public void addPropertyListener(PropertyListener listener)
   {
      if (listener == null)
         throw new NullArgumentException("listener");

      if (listener instanceof BoundPropertyListener)
      {
         addPropertyListener((BoundPropertyListener) listener);
      }
      else
      {
         // only add the listener if it is not in the list already
         if (!unboundListeners.contains(listener))
            unboundListeners.add(listener);
      }
   }

   /**
    * Add a bound property listener.
    *
    * @param listener   Bound property listener to add.
    */
   protected void addPropertyListener(BoundPropertyListener listener)
   {
      // get the bound property name
      String name = listener.getPropertyName();

      // get the bound listener list for the property
      List list = (List) boundListeners.get(name);
      
      // if list is null, then add a new list
      if (list == null)
      {
         list = Collections.synchronizedList(new ArrayList());
         boundListeners.put(name, list);
      }
      
      // if listener is not in the list already, then add it
      if (!list.contains(listener))
      {
         list.add(listener);
         // notify listener that is is bound
         listener.propertyBound(this);
      }
   }

   /**
    * Add an array of property listeners.
    *
    * @param listeners     Array of property listeners to add.
    */
   public void addPropertyListeners(PropertyListener[] listeners)
   {
      if (listeners == null)
         throw new NullArgumentException("listeners");

      for (int i = 0; i < listeners.length; i++)
      {
         addPropertyListener(listeners[i]);
      }
   }

   /**
    * Remove a property listener.
    *
    * @param listener   Property listener to remove.
    * @return           True if listener was removed.
    */
   public boolean removePropertyListener(PropertyListener listener)
   {
      if (listener == null)
         throw new NullArgumentException("listener");

      boolean removed = false;
      if (listener instanceof BoundPropertyListener)
      {
         removed = removePropertyListener((BoundPropertyListener) listener);
      }
      else
      {
         removed = unboundListeners.remove(listener);
      }

      return removed;
   }

   /**
    * Remove a bound property listener.
    *
    * @param listener   Bound property listener to remove.
    * @return           True if listener was removed.
    */
   protected boolean removePropertyListener(BoundPropertyListener listener)
   {
      // get the bound property name
      String name = listener.getPropertyName();
      
      // get the bound listener list for the property
      List list = (List) boundListeners.get(name);
      boolean removed = false;
      if (list != null)
      {
         removed = list.remove(listener);
         
         // notify listener that is was unbound
         if (removed) listener.propertyUnbound(this);
      }
      return removed;
   }


   /////////////////////////////////////////////////////////////////////////
   //                   Listener Notification Methods                     //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Fire a property added event to the given list of listeners.
    *
    * @param list    Listener list.
    * @param event   Property event.
    */
   private void firePropertyAdded(List list, PropertyEvent event)
   {
      if (list == null) return;

      int size = list.size();
      for (int i = 0; i < size; i++)
      {
         PropertyListener listener = (PropertyListener) list.get(i);
         listener.propertyAdded(event);
      }
   }

   /**
    * Fire a property added event to all registered listeners.
    *
    * @param event   Property event.
    */
   protected void firePropertyAdded(PropertyEvent event)
   {
      // fire all bound listeners (if any) first
      if (boundListeners != null)
      {
         List list = (List) boundListeners.get(event.getPropertyName());
         if (list != null)
         {
            firePropertyAdded(list, event);
         }
      }

      // next fire all unbound listeners
      firePropertyAdded(unboundListeners, event);
   }

   /**
    * Fire a property removed event to the given list of listeners.
    *
    * @param list    Listener list.
    * @param event   Property event.
    */
   private void firePropertyRemoved(List list, PropertyEvent event)
   {
      if (list == null) return;

      int size = list.size();
      for (int i = 0; i < size; i++)
      {
         PropertyListener listener = (PropertyListener) list.get(i);
         listener.propertyRemoved(event);
      }
   }

   /**
    * Fire a property removed event to all registered listeners.
    *
    * @param event   Property event.
    */
   protected void firePropertyRemoved(PropertyEvent event)
   {
      // fire all bound listeners (if any) first
      if (boundListeners != null)
      {
         List list = (List) boundListeners.get(event.getPropertyName());
         if (list != null)
         {
            firePropertyRemoved(list, event);
         }
      }

      // next fire all unbound listeners
      firePropertyRemoved(unboundListeners, event);
   }

   /**
    * Fire a property changed event to the given list of listeners.
    *
    * @param list    Listener list.
    * @param event   Property event.
    */
   private void firePropertyChanged(List list, PropertyEvent event)
   {
      if (list == null) return;

      int size = list.size();
      for (int i = 0; i < size; i++)
      {
         PropertyListener listener = (PropertyListener) list.get(i);
         listener.propertyChanged(event);
      }
   }

   /**
    * Fire a property changed event to all listeners.
    *
    * @param event   Property event.
    * @param value   Property value.
    */
   protected void firePropertyChanged(PropertyEvent event)
   {
      // fire all bound listeners (if any) first
      if (boundListeners != null)
      {
         List list = (List) boundListeners.get(event.getPropertyName());
         if (list != null)
         {
            firePropertyChanged(list, event);
         }
      }

      // next fire all unbound listeners
      firePropertyChanged(unboundListeners, event);
   }


   /////////////////////////////////////////////////////////////////////////
   //                       Property Loading Methods                      //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Make a optionaly prefixed property name.
    *
    * @param base    Base property name.
    * @param prefix  Optional prefix (can be null).
    * @return        Property name.
    */
   protected String makePrefixedPropertyName(String base, String prefix)
   {
      String name = base;

      if (prefix != null)
      {
         StringBuffer buff = new StringBuffer(base);
         if (prefix != null)
         {
            buff.insert(0, PROPERTY_NAME_SEPARATOR);
            buff.insert(0, prefix);
         }
         return buff.toString();
      }

      return name;
   }

   /**
    * Load properties from a map.
    *
    * @param prefix  Prefix to append to all map keys (or null).
    * @param map     Map containing properties to load.
    */
   public void load(String prefix, Map map) throws PropertyException
   {
      // prefix can be null
      if (map == null)
         throw new NullArgumentException("map");

      // set properties for each key in map
      Iterator iter = map.keySet().iterator();
      while (iter.hasNext())
      {
         // make a string key with optional prefix
         String key = String.valueOf(iter.next());
         String name = makePrefixedPropertyName(key, prefix);
         String value = String.valueOf(map.get(name));

         // set the property
         setProperty(name, value);
      }
   }

   /**
    * Load properties from a map.
    *
    * @param map  Map containing properties to load.
    */
   public void load(Map map) throws PropertyException
   {
      load(null, map);
   }

   /**
    * Load properties from a PropertyReader.
    *
    * @param reader  PropertyReader to read properties from.
    */
   public void load(PropertyReader reader) throws PropertyException, IOException
   {
      if (reader == null)
         throw new NullArgumentException("reader");

      load(reader.readProperties());
   }

   /**
    * Load properties from a PropertyReader specifed by the given class name.
    *
    * @param className     Class name of a PropertyReader to read from.
    */
   public void load(String className) throws PropertyException, IOException
   {
      if (className == null)
         throw new NullArgumentException("className");

      PropertyReader reader = null;

      try
      {
         Class type = Class.forName(className);
         reader = (PropertyReader) type.newInstance();
      }
      catch (Exception e)
      {
         throw new PropertyException(e);
      }
         
      // load the properties from the source
      load(reader);
   }


   /////////////////////////////////////////////////////////////////////////
   //                    Direct Property Access Methods                   //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Set a property.
    *
    * <p>Returns Object instead of String due to limitations with
    *    <code>java.util.Properties</code>.
    *
    * @param name    Property name.
    * @param value   Property value.
    * @return        Previous property value or null.
    */
   public Object setProperty(String name, String value)
   {
      updateJndiCache(name, value);
      return put(name, value);
   }

   public String getProperty(String name)
   {
      Object value = jndiMap.get(name);
      if (value != null)
      {
         // key was in the map
         return (value == NULL_VALUE) ? null : (String) value;
      }
      return super.getProperty(name);
   }

   /**
    * Remove a property.
    *
    * @param name    Property name.
    * @return        Removed property value or null.
    */
   public String removeProperty(String name)
   {
      return (String) remove(name);
   }


   /**
    * Make an indexed property name.
    *
    * @param base    Base property name.
    * @param index   Property index.
    * @return        Indexed property name.
    */
   protected String makeIndexPropertyName(String base, int index)
   {
      return base + PROPERTY_NAME_SEPARATOR + index;
   }

   /**
    * Get an array style property.
    *
    * <p>Array properties are specified as:
    *    <code>base_property_name.<b>INDEX</b>.
    *
    * <p>Indexes begin with zero and must be contiguous.  A break in
    *    continuity signals the end of the array.
    * 
    * @param base          Base property name.
    * @param defaultValues Default property values.
    * @return              Array of property values or default.
    */
   public String[] getArrayProperty(String base, String[] defaultValues)
   {
      if (base == null)
         throw new NullArgumentException("base");

      // create a new list to store indexed values into
      List list = new LinkedList();

      int i = 0;
      while (true)
      {
         // make the index property name
         String name = makeIndexPropertyName(base, i);

         // see if there is a value for this property
         String value = getProperty(name);

         if (value != null)
         {
            list.add(value);
         }
         else if (i >= 0)
         {
            break; // no more index properties
         }

         i++;
      }

      String values[] = defaultValues;

      // if the list is not empty, then return it as an array
      if (list.size() != 0)
      {
         values = (String[]) list.toArray(new String[list.size()]);
      }

      return values;
   }

   /**
    * Get an array style property.
    *
    * @param name       Property name.
    * @return           Array of property values or empty array.
    */
   public String[] getArrayProperty(String name)
   {
      return getArrayProperty(name, EMPTY_ARRAY_PROPERTY);
   }


   /////////////////////////////////////////////////////////////////////////
   //                          Iterator Methods                           //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Return an iterator over all contained property names.
    *
    * @return     Property name iterator.
    */
   public Iterator names()
   {
      return keySet().iterator();
   }


   /////////////////////////////////////////////////////////////////////////
   //                            State Methods                            //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Check if this map contains a given property.
    *
    * @param name    Property name.
    * @return        True if contains property.
    */
   public boolean containsProperty(String name)
   {
      return containsKey(name);
   }


   /////////////////////////////////////////////////////////////////////////
   //                        Property Group Methods                       //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Get a property group for the given property base.
    *
    * @param basename   Base property name.
    * @return           Property group.
    */
   public PropertyGroup getPropertyGroup(String basename)
   {
      return new PropertyGroup(basename, this);
   }

   /**
    * Get a property group for the given property base at the given index.
    *
    * @param basename   Base property name.
    * @param index      Array property index.
    * @return           Property group.
    */
   public PropertyGroup getPropertyGroup(String basename, int index)
   {
      String name = makeIndexPropertyName(basename, index);
      return getPropertyGroup(name);
   }


   /////////////////////////////////////////////////////////////////////////
   //                         Serialization Methods                       //
   /////////////////////////////////////////////////////////////////////////

   private void readObject(ObjectInputStream stream)
      throws IOException, ClassNotFoundException
   {
      // reset the listener lists
      init();

      stream.defaultReadObject();
   }

   private void writeObject(ObjectOutputStream stream)
      throws IOException
   {
      stream.defaultWriteObject();
   }
}
