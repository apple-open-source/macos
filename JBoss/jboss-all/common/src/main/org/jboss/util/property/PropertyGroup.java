/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import java.util.Properties;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.Iterator;

import org.jboss.util.NullArgumentException;

/**
 * This is a helper class to access a group of properties with out having to
 * refer to their full names.
 *
 * <p>This class needs more work to be fully functional.  It should suffice
 *    for adding property listeners and getting/setting property values,
 *    but other activies might not work out so well.
 *
 * @version <tt>$Revision: 1.2 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class PropertyGroup
   extends PropertyMap
{
   /** Base property name */
   protected final String basename;

   /**
    * Construct a <tt>PropertyGroup</tt>.
    *
    * @param basename   Base property name.
    * @param container  Property container.
    *
    * @throws NullArgumentException    Basename is <tt>null</tt>.
    */
   public PropertyGroup(final String basename, final Properties container) {
      super(container);
      
      if (basename == null)
         throw new NullArgumentException("basename");

      this.basename = basename;
   }

   /**
    * Get the base property name for this group.
    *
    * @return  Base property name.
    */
   public final String getBaseName() {
      return basename;
   }

   /**
    * Make a fully qualified property name.
    *
    * @param suffix  Property name suffix.
    */      
   private String makePropertyName(final String suffix) {
      return basename + PropertyMap.PROPERTY_NAME_SEPARATOR + suffix;
   }

   /**
    * Make a fully qualified property name.
    *
    * @param suffix  Property name suffix.
    */      
   private String makePropertyName(final Object suffix) {
      return makePropertyName(String.valueOf(suffix));
   }
   

   /////////////////////////////////////////////////////////////////////////
   //                         Properties Overrides                        //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Check if this <tt>PropertyMap</tt> contains a given property name.
    *
    * @param name    Property name.
    * @return        True if property map or defaults contains key.
    */
   public boolean containsKey(final Object name) {
      if (name == null)
         throw new NullArgumentException("name");
         
      return super.containsKey(makePropertyName(name));
   }

   /**
    * Set a property.
    *
    * @param name    Property name.
    * @param value   Property value.
    * @return        Previous property value or <tt>null</tt>.
    */
   public Object put(final Object name, final Object value) {
      if (name == null)
         throw new NullArgumentException("name");
      
      return super.put(makePropertyName(name), value);
   }

   /**
    * Get a property
    *
    * @param name    Property name.
    * @return        Property value or <tt>null</tt>.
    */
   public Object get(final Object name) {
      if (name == null)
         throw new NullArgumentException("name");

      return super.get(makePropertyName(name));
   }

   /**
    * Remove a property.
    *
    * @param name    Property name.
    * @return        Removed property value.
    */
   public Object remove(final Object name) {
      if (name == null)
         throw new NullArgumentException("name");

      return super.remove(makePropertyName(name));
   }

   /**
    * Returns an entry set for all properties in this group.
    *
    * <p>
    * This is currently ver inefficient, but should get the
    * job done for now.
    */
   public Set entrySet()
   {
      final Set superSet = super.entrySet(true);
         
      return new java.util.AbstractSet()
         {
            private boolean isInGroup(Map.Entry entry)
            {
               String key = (String)entry.getKey();
               return key.startsWith(basename);
            }

            public int size()
            {
               Iterator iter = superSet.iterator();
               int count = 0;
               while (iter.hasNext()) {
                  Map.Entry entry = (Map.Entry)iter.next();
                  if (isInGroup(entry)) {
                     count++;
                  }
               }

               return count;
            }

            public Iterator iterator()
            {
               return new Iterator()
                  {
                     private Iterator iter = superSet.iterator();
                     private Object next;
                     
                     public boolean hasNext()
                     {
                        if (next != null)
                           return true;

                        while (next == null) {
                           if (iter.hasNext()) {
                              Map.Entry entry = (Map.Entry)iter.next();
                              if (isInGroup(entry)) {
                                 next = entry;
                                 return true;
                              }
                           }
                           else {
                              break;
                           }
                        }

                        return false;
                     }

                     public Object next()
                     {
                        if (next == null)
                           throw new java.util.NoSuchElementException();

                        Object obj = next;
                        next = null;

                        return obj;
                     }

                     public void remove()
                     {
                        iter.remove();
                     }
                  };
            }
         };
   }

   
   /////////////////////////////////////////////////////////////////////////
   //                     Property Listener Overrides                     //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Add a bound property listener.
    *
    * <p>Generates a fully qualified property name and adds the listener
    *    under that name.
    *
    * @param listener   Bound property listener to add.
    */
   protected void addPropertyListener(final BoundPropertyListener listener) {
      // get the bound property name
      String name = makePropertyName(listener.getPropertyName());

      // get the bound listener list for the property
      List list = (List)boundListeners.get(name);
      
      // if list is null, then add a new list
      if (list == null) {
         list = new ArrayList();
         boundListeners.put(name, list);
      }
      
      // if listener is not in the list already, then add it
      if (! list.contains(listener)) {
         list.add(listener);
         // notify listener that is is bound
         listener.propertyBound(this);
      }
   }

   /**
    * Remove a bound property listener.
    *
    * <p>Generates a fully qualified property name and removes the listener
    *    under that name.
    *
    * @param listener   Bound property listener to remove.
    * @return           True if listener was removed.
    */
   protected boolean removePropertyListener(final BoundPropertyListener listener) {
      // get the bound property name
      String name = makePropertyName(listener.getPropertyName());
      
      // get the bound listener list for the property
      List list = (List)boundListeners.get(name);
      boolean removed = false;
      if (list != null) {
         removed = list.remove(listener);
         
         // notify listener that is was unbound
         if (removed) listener.propertyUnbound(this);
      }

      return removed;
   }
}
