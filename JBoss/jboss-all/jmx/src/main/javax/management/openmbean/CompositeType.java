/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.util.Collections;
import java.util.Iterator;
import java.util.Set;
import java.util.TreeMap;

/**
 * The CompositeType is an OpenType that describes CompositeData.
 *
 * @see CompositeData
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public class CompositeType
   extends OpenType
{

   // Attributes ----------------------------------------------------

   /**
    * Item names to descriptions
    */
   private TreeMap nameToDescription;

   /**
    * Item names to open types
    */
   private TreeMap nameToType;

   /**
    * Cached hash code
    */
   private transient int cachedHashCode = 0;

   /**
    * Cached string representation
    */
   private transient String cachedToString = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID = -5366242454346948798L;

   // Constructors --------------------------------------------------

   /**
    * Construct a composite type. The parameters are checked for validity.<p>
    *
    * The three arrays are internally copied. Future changes to these
    * arrays do not alter the composite type.<p>
    *
    * getClassName() returns javax.management.openbean.CompositeData<p>
    *
    * @param typeName the name of the composite type, cannot be null or
    *        empty
    * @param description the human readable description of the composite type, 
    *        cannot be null or empty
    * @param itemNames the names of the items described by this type. Cannot
    *        be null, must contain at least one element, the elements cannot
    *        be null or empty. The order of the items is unimportant when
    *        determining equality.
    * @param itemDescriptions the human readable descriptions of the items
    *        in the same order as the itemNames, cannot be null must have the
    *        same number of elements as the itemNames. The elements cannot
    *        be null or empty.
    * @param itemTypes the OpenTypes of the items in the same order as the
    *        item names, cannot be null must have the
    *        same number of elements as the itemNames. The elements cannot
    *        be null.
    * @exception OpenDataException when itemNames contains a duplicate name.
    *            The names are case sensitive, leading and trailing whitespace
    *            is ignored.
    * @exception IllegalArgumentException when a parameter does not match
    *            what is described above.
    */
   public CompositeType(String typeName, String description, 
                        String[] itemNames, String[] itemDescriptions,
                        OpenType[] itemTypes)
      throws OpenDataException
   {
      super(CompositeData.class.getName(), typeName, description);
      if (itemNames == null)
         throw new IllegalArgumentException("null itemNames");
      if (itemDescriptions == null)
         throw new IllegalArgumentException("null itemDescriptions");
      if (itemTypes == null)
         throw new IllegalArgumentException("null itemTypes");
      if (itemNames.length != itemDescriptions.length)
         throw new IllegalArgumentException("wrong number of itemDescriptions");
      if (itemNames.length != itemTypes.length)
         throw new IllegalArgumentException("wrong number of itemTypes");
      nameToDescription = new TreeMap();
      nameToType = new TreeMap();
      for (int i = 0; i < itemNames.length; i++)
      {
          if (itemNames[i] == null)
             throw new IllegalArgumentException("null item name " + i);
          String itemName = itemNames[i].trim();
          if (itemName.length() == 0)
             throw new IllegalArgumentException("empty item name " + i);
          if (nameToDescription.containsKey(itemName))
             throw new OpenDataException("duplicate item name " + itemName);
          if (itemDescriptions[i] == null)
             throw new IllegalArgumentException("null item description " + i);
          String itemDescription = itemDescriptions[i].trim();
          if (itemDescription.length() == 0)
             throw new IllegalArgumentException("empty item description " + i);
          if (itemTypes[i] == null)
             throw new IllegalArgumentException("null item type " + i);
          nameToDescription.put(itemName, itemDescription);
          nameToType.put(itemName, itemTypes[i]);
      }
   }

   // Public --------------------------------------------------------

   /**
    * Determine whether this CompositeType contains the itemName
    *
    * @param itemName the item name
    * @return true when it does, false otherwise
    */
   public boolean containsKey(String itemName)
   {
      if (itemName == null || itemName.length() == 0)
         return false;
      return nameToDescription.containsKey(itemName);
   }

   /**
    * Retrieve the description for an item name
    *
    * @param itemName the item name
    * @return the description or null when there is no such item name
    */
   public String getDescription(String itemName)
   {
      return (String) nameToDescription.get(itemName);
   }

   /**
    * Retrieve the open type for an item name
    *
    * @param itemName the item name
    * @return the open type or null when there is no such item name
    */
   public OpenType getType(String itemName)
   {
      return (OpenType) nameToType.get(itemName);
   }

   /**
    * Retrieve an unmodifiable Set view of all the item names in
    * ascending order.
    *
    * @return the Set
    */
   public Set keySet()
   {
      return Collections.unmodifiableSet(nameToDescription.keySet());
   }

   // OpenType Overrides --------------------------------------------

   /**
    * Determines whether the object is a value of the this composite type.<p>
    *
    * The object must not be null and it must be an instance of
    * javax.management.openbean.CompositeData. The CompositeType of the
    * CompositeData have equality with this CompositeType.
    *
    * @param obj the object to test
    * @return the true when the above condition is satisfied, false otherwise
    */
   public boolean isValue(Object obj)
   {
      if (obj == null || !(obj instanceof CompositeData))
         return false;
      return equals(((CompositeData) obj).getCompositeType());
   }

   /**
    * Tests for equality with another composite type<p>
    *
    * The type names must be equal.<br>
    * The item names and types are equal.
    *
    * @param obj the other composite type to test
    * @return the true when the above condition is satisfied, false otherwise
    */
   public boolean equals(Object obj)
   {
      if (this == obj)
         return true;
      if (obj == null || (obj instanceof CompositeType) == false)
         return false;
      CompositeType other = (CompositeType) obj;
      if (this.getTypeName().equals(other.getTypeName()) == false)
         return false;
      Iterator thisNames = this.keySet().iterator();
      Iterator otherNames = other.keySet().iterator();
      while(thisNames.hasNext() && otherNames.hasNext())
      {
         String thisName = (String) thisNames.next();
         String otherName = (String) otherNames.next();
         if (thisName.equals(otherName) == false)
            return false;
         if (this.getType(thisName).equals(other.getType(otherName)) == false)
            return false;
      }
      if (thisNames.hasNext() || otherNames.hasNext())
         return false;
      return true;
   }

   public int hashCode()
   {
      if (cachedHashCode != 0)
         return cachedHashCode;
      cachedHashCode = getTypeName().hashCode();
      for (Iterator i = nameToType.values().iterator(); i.hasNext();)
         cachedHashCode += i.next().hashCode();
      for (Iterator i = nameToDescription.keySet().iterator(); i.hasNext();)
         cachedHashCode += i.next().hashCode();
      return cachedHashCode;
   }

   public String toString()
   {
      if (cachedToString != null)
         return cachedToString;
      StringBuffer buffer = new StringBuffer(getClass().getName());
      buffer.append("\n");
      Iterator thisNames = keySet().iterator();
      while(thisNames.hasNext())
      {
         String thisName = (String) thisNames.next();
         buffer.append("name=");
         buffer.append(thisName);
         buffer.append(" type=");
         buffer.append(getType(thisName));
         if (thisNames.hasNext())
           buffer.append("\n");
      }
      cachedToString = buffer.toString();
      return cachedToString;
   }
}
