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

import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * An implementation of TabularData.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class TabularDataSupport
   implements Cloneable, Map, Serializable, TabularData
{
   // Constants -----------------------------------------------------------------

   private static final long serialVersionUID = 5720150593236309827L;
   private static final ObjectStreamField[] serialPersistentFields =
      new ObjectStreamField[]
      {
         new ObjectStreamField("dataMap",      Map.class),
         new ObjectStreamField("tabularType",  TabularType.class),
      };

   // Attributes ----------------------------------------------------

   /**
    * The data map of this tabular data
    */
   private Map dataMap;

   /**
    * The tabular type of the tabular data
    */
   private TabularType tabularType;

   /**
    * The index names
    */
   private transient String[] indexNames;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct Tabular Data with an initial capacity of 101 and a load
    * factor of 0.75
    *
    * @param tabularType the tabular type of the data
    * @exception IllegalArgumentException for a null argument
    */
   public TabularDataSupport(TabularType tabularType)
   {
      this(tabularType, 101, 0.75f);
   }

   /**
    * Construct Tabular Data
    *
    * @param tabularType the tabular type of the data
    * @param initialCapacity the initial capacity of the map
    * @param loadFactor the load factory of the map
    * @exception IllegalArgumentException for a null argument
    */
   public TabularDataSupport(TabularType tabularType, int initialCapacity,
                             float loadFactor)
   {
      init(new HashMap(initialCapacity, loadFactor), tabularType);
   }

   // Public --------------------------------------------------------

   // Tabular Data Implementation -----------------------------------

   public TabularType getTabularType()
   {
      return tabularType;
   }

   public Object[] calculateIndex(CompositeData value)
   {
      validateCompositeData(value);
      return value.getAll(indexNames);
   }

   public void clear()
   {
      dataMap.clear();
   }

   public boolean containsKey(Object[] key)
   {
      if (key == null)
         return false;
      return dataMap.containsKey(Arrays.asList(key));
   }

   public boolean containsValue(CompositeData value)
   {
      // javadoc says check the value, but this is done at addition
      return dataMap.containsValue(value);
   }

   public CompositeData get(Object[] key)
   {
      validateKey(key);
      return (CompositeData) dataMap.get(Arrays.asList(key));
   }

   public boolean isEmpty()
   {
      return dataMap.isEmpty();
   }

   public Set keySet()
   {
      return dataMap.keySet();
   }

   public void put(CompositeData value)
   {
      List index = Arrays.asList(calculateIndex(value));
      if (dataMap.containsKey(index))
         throw new KeyAlreadyExistsException("The index is already used " + index);
      dataMap.put(index, value);
   }

   public void putAll(CompositeData[] values)
   {
      if (values == null)
         return;
      HashSet keys = new HashSet();
      for (int i = 0; i < values.length; i++)
      {
         List index = Arrays.asList(calculateIndex(values[i]));
         if (keys.contains(index))
            throw new KeyAlreadyExistsException("Duplicate index in values " +
               index + " for value " + values[i]);
         keys.add(index);
         if (dataMap.containsKey(index))
            throw new KeyAlreadyExistsException("Index already used " +
               index + " for value " + values[i]);
      }
      for (int i = 0; i < values.length; i++)
         put(values[i]);
   }

   public CompositeData remove(Object[] key)
   {
      validateKey(key);
      return (CompositeData) dataMap.remove(Arrays.asList(key));
   }

   public int size()
   {
      return dataMap.size();
   }

   public Collection values()
   {
      return dataMap.values();
   }

   // Map Implementation --------------------------------------------

   public boolean containsKey(Object key)
   {
      if (key == null || (key instanceof Object[]) == false)
         return false;
      return containsKey((Object[]) key);
   }

   public boolean containsValue(Object value)
   {
      return dataMap.containsValue(value);
   }

   public Set entrySet()
   {
      return dataMap.entrySet();
   }

   public Object get(Object key)
   {
      return get((Object[]) key);
   }

   public Object put(Object key, Object value)
   {
      put((CompositeData) value);
      return value;
   }

   public void putAll(Map t)
   {
      if (t == null)
         return;
      CompositeData[] data = new CompositeData[ t.size() ];
      int count = 0;
      for (Iterator i = t.values().iterator(); i.hasNext();)
         data[count++] = (CompositeData) i.next();
      putAll(data);
   }

   public Object remove(Object key)
   {
      return remove((Object[]) key);
   }

   // Cloneable Implentation ----------------------------------------

   public Object clone()
   {
      try
      {
         TabularDataSupport result = (TabularDataSupport) super.clone();
         result.dataMap = (Map) ((HashMap) this.dataMap).clone();
         return result;
      }
      catch (CloneNotSupportedException e)
      {
         throw new RuntimeException("Unexpected clone not supported exception.");
      }
   }

   // Serializable Implementation -----------------------------------

   private void readObject(ObjectInputStream in)
      throws IOException, ClassNotFoundException
   {
      ObjectInputStream.GetField getField = in.readFields();
      Map dataMap = (Map) getField.get("dataMap", null);
      TabularType tabularType = (TabularType) getField.get("tabularType", null);
      try
      {
         init(dataMap, tabularType);
      }
      catch (Exception e)
      {
         throw new StreamCorruptedException(e.toString());
      }
   }

   // Object Overrides ----------------------------------------------

   public boolean equals(Object obj)
   {
      if (obj == null || (obj instanceof TabularData) == false)
         return false;
      TabularData other = (TabularData) obj;
      if (tabularType.equals(other.getTabularType()) == false)
         return false;
      if (size() != other.size())
         return false;
      for (Iterator i = dataMap.entrySet().iterator(); i.hasNext();)
      {
         Entry entry = (Entry) i.next();
         Object[] indexes = ((List) entry.getKey()).toArray();
         Object thisValue = entry.getValue();
         Object otherValue = other.get(indexes);
         if (thisValue == null && otherValue != null)
            return false;
         if (thisValue.equals(otherValue) == false)
            return false;
      }
      return true;
   }

   public int hashCode()
   {
      int hash = tabularType.hashCode();
      for (Iterator i = dataMap.values().iterator(); i.hasNext();)
         hash += i.next().hashCode();
      return hash;
   }

   public String toString()
   {
      StringBuffer buffer = new StringBuffer(getClass().getName());
      buffer.append(": tabularType=[");
      buffer.append(getTabularType());
      buffer.append("] mappings=[");
      Iterator entries = dataMap.entrySet().iterator();
      while(entries.hasNext())
      {
         Entry entry = (Entry) entries.next(); 
         buffer.append(entry.getKey());
         buffer.append("=");
         buffer.append(entry.getValue());
         if (entries.hasNext())
            buffer.append(", ");
      }
      buffer.append("]");
      return buffer.toString();
   }

   // Private -------------------------------------------------------

   /**
    * Initialise the tabular data
    *
    * @param dataMap the data
    * @param tabularType the tabular type of the data
    * @exception IllegalArgumentException for a null
    */
   private void init(Map dataMap, TabularType tabularType)
   {
      if (dataMap == null)
         throw new IllegalArgumentException("null dataMap");
      if (tabularType == null)
         throw new IllegalArgumentException("null tabularType");

      this.dataMap = dataMap;
      this.tabularType = tabularType;
      List indexNameList = tabularType.getIndexNames();
      this.indexNames = (String[]) indexNameList.toArray(new String[indexNameList.size()]);
   }

   /**
    * Validate the composite type against the row type
    *
    * @param value the composite data
    * @exception NullPointerException for a null value
    * @exception InvalidOpenTypeException if the value is not valid for the 
    *            tabular data's row type
    */
   private void validateCompositeData(CompositeData value)
   {
      if (value == null)
         throw new NullPointerException("null value");
      if (value.getCompositeType().equals(tabularType.getRowType()) == false)
         throw new InvalidOpenTypeException("value has composite type " +
            value.getCompositeType() + " expected row type " +
            tabularType.getRowType());
   }

   /**
    * Validate the key against the row type
    *
    * @param key the key to check
    * @exception NullPointerException for a null key
    * @exception InvalidKeyException if the key is not valid for the 
    *            tabular data's row type
    */
   private void validateKey(Object[] key)
   {
      if (key == null || key.length == 0)
         throw new NullPointerException("null key");
      if (key.length != indexNames.length)
         throw new InvalidKeyException("key has " + key.length + " elements, " +
            "should be " + indexNames.length);
      for (int i = 0; i < key.length; i++)
      {
         OpenType openType = tabularType.getRowType().getType(indexNames[i]);
         if (openType.isValue(key[i]) == false)
            throw new InvalidKeyException("key element " + i +
               " " + key + " is not a value for " +
               openType);
      }
   }
}

