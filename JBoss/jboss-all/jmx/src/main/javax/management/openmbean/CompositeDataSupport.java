/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectStreamField;
import java.io.Serializable;
import java.io.StreamCorruptedException;

import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Map;
import java.util.SortedMap;
import java.util.TreeMap;

/**
 * An implementation of CompositeData.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class CompositeDataSupport
   implements CompositeData, Serializable
{
   // Constants -----------------------------------------------------------------

   private static final long serialVersionUID = 8003518976613702244L;
   private static final ObjectStreamField[] serialPersistentFields =
      new ObjectStreamField[]
      {
         new ObjectStreamField("contents",      SortedMap.class),
         new ObjectStreamField("compositeType", CompositeType.class),
      };

   // Attributes ----------------------------------------------------

   /**
    * The contents of the composite data
    */
   private SortedMap contents;

   /**
    * The composite type of the composite data
    */
   private CompositeType compositeType;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct Composite Data 
    *
    * @param compositeType the composite type of the data
    * @param itemNames the names of the values
    * @param itemValues the values
    * @exception IllegalArgumentException for a null or empty argument
    * @exception OpenDataException when the items do not match the
    *            CompositeType
    */
   public CompositeDataSupport(CompositeType compositeType,
                               String[] itemNames,
                               Object[] itemValues)
      throws OpenDataException
   {
      if (compositeType == null)
         throw new IllegalArgumentException("null compositeType");
      if (itemNames == null)
         throw new IllegalArgumentException("null itemNames");
      if (itemValues == null)
         throw new IllegalArgumentException("null itemValues");
      if (itemNames.length == 0)
         throw new IllegalArgumentException("empty itemNames");
      if (itemValues.length == 0)
         throw new IllegalArgumentException("empty itemValues");
      if (itemNames.length != itemValues.length)
         throw new IllegalArgumentException("itemNames has size " + itemNames.length +
            " but itemValues has size " + itemValues.length);
      int compositeNameSize = compositeType.keySet().size();
      if (itemNames.length != compositeNameSize)
         throw new OpenDataException("itemNames has size " + itemNames.length +
            " but composite type has size " + compositeNameSize);

      this.compositeType = compositeType;
      contents = new TreeMap();

      HashSet names = new HashSet();
      for (int i = 0; i < itemNames.length; i++)
      {
         if (itemNames[i] == null || itemNames[i].length() == 0)
            throw new IllegalArgumentException("Item name " + i + " is null or empty");
         if (names.contains(itemNames[i]))
            throw new OpenDataException("duplicate item name " + itemNames[i]);
         OpenType openType = compositeType.getType(itemNames[i]);
         if (openType == null)
            throw new OpenDataException("item name not in composite type " + itemNames[i]);
         if (itemValues[i] != null && openType.isValue(itemValues[i]) == false)
            throw new OpenDataException("item value " + itemValues[i] + " for item name " +
               itemNames[i] + " is not a " + openType);
         contents.put(itemNames[i], itemValues[i]);
      }
   }

   /**
    * Construct Composite Data 
    *
    * @param compositeType the composite type of the data
    * @param items map of strings to values
    * @exception IllegalArgumentException for a null or empty argument
    * @exception OpenDataException when the items do not match the
    *            CompositeType
    * @exception ArrayStoreException when a key to the map is not a String
    */
   public CompositeDataSupport(CompositeType compositeType, Map items)
      throws OpenDataException
   {
      init(compositeType, items);
   }

   // Public --------------------------------------------------------

   // Composite Data Implementation ---------------------------------

   public CompositeType getCompositeType()
   {
      return compositeType;
   }

   public Object get(String key)
   {
      validateKey(key);
      return contents.get(key);
   }

   public Object[] getAll(String[] keys)
   {
      if (keys == null)
         return new Object[0];
      Object[] result = new Object[keys.length];
      for (int i = 0; i < keys.length; i++)
      {
         validateKey(keys[i]);
         result[i] = contents.get(keys[i]);
      }
      return result;
   }

   public boolean containsKey(String key)
   {
      if (key == null || key.length() == 0)
         return false;
      return contents.containsKey(key);
   }

   public boolean containsValue(Object value)
   {
      return contents.containsValue(value);
   }

   public Collection values()
   {
      return Collections.unmodifiableCollection(contents.values());
   }

   // Serializable Implementation -----------------------------------

   private void readObject(ObjectInputStream in)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = in.readFields();
      SortedMap contents = (SortedMap) getField.get("contents", null);
      CompositeType compositeType = (CompositeType) getField.get("compositeType", null);
      try
      {
         init(compositeType, contents);
      }
      catch (Exception e)
      {
         throw new StreamCorruptedException(e.toString());
      }
   }

   // Object Overrides ----------------------------------------------

   public boolean equals(Object obj)
   {
      if (obj == null || (obj instanceof CompositeData) == false)
         return false;
      CompositeData other = (CompositeData) obj;
      if (compositeType.equals(other.getCompositeType()) == false)
         return false;
      for (Iterator i = contents.keySet().iterator(); i.hasNext();)
      {
         String key = (String) i.next();
         Object thisValue = this.get(key);
         Object otherValue = other.get(key);
         if (thisValue == null && otherValue != null)
            return false;
         if (thisValue.equals(otherValue) == false)
            return false;
      }
      return true;
   }

   public int hashCode()
   {
      int hash = compositeType.hashCode();
      for (Iterator i = contents.values().iterator(); i.hasNext();)
         hash += i.next().hashCode();
      return hash;
   }

   public String toString()
   {
      StringBuffer buffer = new StringBuffer(getClass().getName());
      buffer.append(": compositeType=[");
      buffer.append(getCompositeType());
      buffer.append("] mappings=[");
      Iterator keys = compositeType.keySet().iterator();
      while(keys.hasNext())
      {
         Object key = keys.next();
         buffer.append(key);
         buffer.append("=");
         buffer.append(contents.get(key));
         if (keys.hasNext())
            buffer.append(", ");
      }
      buffer.append("]");
      return buffer.toString();
   }

   // Private -------------------------------------------------------

   /**
    * Initialise the composite data
    *
    * @param compositeType the composite type of the data
    * @param items map of strings to values
    * @exception IllegalArgumentException for a null or empty argument
    * @exception OpenDataException when the items do not match the
    *            CompositeType
    * @exception ArrayStoreException when a key to the map is not a String
    */
   private void init(CompositeType compositeType, Map items)
      throws OpenDataException
   {
      if (compositeType == null)
         throw new IllegalArgumentException("null compositeType");
      if (items == null)
         throw new IllegalArgumentException("null items");
      if (items.size() == 0)
         throw new IllegalArgumentException("empty items");
      int compositeNameSize = compositeType.keySet().size();
      if (items.size() != compositeNameSize)
         throw new OpenDataException("items has size " + items.size() +
            " but composite type has size " + compositeNameSize);

      this.compositeType = compositeType;
      contents = new TreeMap();

      for (Iterator i = items.keySet().iterator(); i.hasNext();)
      {
         Object next = i.next();
         if (next != null && (next instanceof String) == false)
            throw new ArrayStoreException("key is not a String " + next);
         String key = (String) next;
         if (key == null || key.length() == 0)
            throw new IllegalArgumentException("Key is null or empty");
         OpenType openType = compositeType.getType(key);
         if (openType == null)
            throw new OpenDataException("item name not in composite type " + key);
         Object value = items.get(key);
         if (value != null && openType.isValue(value) == false)
            throw new OpenDataException("item value " + value + " for item name " +
               key + " is not a " + openType);
         contents.put(key, value);
      }
   }

   /**
    * Validates the key against the composite type
    *
    * @param key the key to check
    * @exception IllegalArgumentException for a null or empty key
    * @exception InvalidKeyException if the key not a valid item name for the composite type
    */
   public void validateKey(String key)
      throws InvalidKeyException
   {
      if (key == null || key.length() == 0)
         throw new IllegalArgumentException("null or empty key");
      if (compositeType.containsKey(key) == false)
         throw new InvalidKeyException("no such item name " + key + " for composite type " +
            compositeType);
   }
}

