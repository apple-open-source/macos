/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

/**
 * The TabularType is an OpenType that describes TabularData.
 *
 * @see TabularData
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.2 $
 *
 */
public class TabularType
   extends OpenType
{
   // Attributes ----------------------------------------------------

   /**
    * The open type of the rows
    */
   private CompositeType rowType;

   /**
    * Index names
    */
   private List indexNames;

   /**
    * Cached hash code
    */
   private transient int cachedHashCode = 0;

   /**
    * Cached string representation
    */
   private transient String cachedToString = null;

   // Static --------------------------------------------------------

   private static final long serialVersionUID = 6554071860220659261L;

   // Constructors --------------------------------------------------

   /**
    * Construct a tabular type. The parameters are checked for validity.<p>
    *
    * getClassName() returns javax.management.openbean.TabularData<p>
    *
    * @param typeName the name of the tabular type, cannot be null or
    *        empty
    * @param description the human readable description of the tabular type, 
    *        cannot be null or empty
    * @param rowType the type of the row elements in the tabular data, cannot
    *        be null
    * @param indexNames the names of the item values that uniquely index each
    *        row element in the tabular data, cannot be null or empty. Each
    *        element must be an item name in the rowType, nul or empty is not
    *        allowed. The order of the item names in this parameter is used
    *        by {@link TabularData#get} and {@link TabularData#remove} the 
    *        TabularData to match the array of values to items.
    * @exception OpenDataException when an element of indexNames is not defined
    *            in rowType.
    * @exception IllegalArgumentException when a parameter does not match
    *            what is described above.
    */
   public TabularType(String typeName, String description, 
                      CompositeType rowType, String[] indexNames)
      throws OpenDataException
   {
      super(TabularData.class.getName(), typeName, description);
      if (rowType == null)
         throw new IllegalArgumentException("null rowType");
      if (indexNames == null || indexNames.length == 0)
         throw new IllegalArgumentException("null or empty indexNames");
      this.rowType = rowType;
      this.indexNames = new ArrayList();
      for (int i = 0; i < indexNames.length; i++)
      {
          if (indexNames[i] == null)
             throw new IllegalArgumentException("null index name " + i);
          String indexName = indexNames[i].trim();
          if (indexName.length() == 0)
             throw new IllegalArgumentException("empty index name " + i);
          if (rowType.containsKey(indexName) == false)
             throw new OpenDataException("no item name " + indexName);
          this.indexNames.add(indexName);
      }
   }

   // Public --------------------------------------------------------

   /**
    * Retrieve the row type
    *
    * @return the row type
    */
   public CompositeType getRowType()
   {
      return rowType;
   }

   /**
    * Retrieve an unmodifiable list of index names in the same order as
    * passed to the constructor.
    *
    * @return the index names
    */
   public List getIndexNames()
   {
      return Collections.unmodifiableList(indexNames);
   }

   // OpenType Overrides --------------------------------------------

   /**
    * Determines whether the object is a value of the this tabular type.<p>
    *
    * The object must not be null and it must be an instance of
    * javax.management.openbean.TabularData. The TabularType of the
    * TabularData have equality with this TabularType.
    *
    * @param obj the object to test
    * @return the true when the above condition is satisfied, false otherwise
    */
   public boolean isValue(Object obj)
   {
      if (obj == null || !(obj instanceof TabularData))
         return false;
      TabularType other = ((TabularData) obj).getTabularType();
      return equals(other);
   }

   /**
    * Tests for equality with another composite type<p>
    *
    * The type names must be equal.<br>
    * The row types are equal<br>
    * The index names are the same and in the same order.
    *
    * @param obj the other tabular type to test
    * @return the true when the above condition is satisfied, false otherwise
    */
   public boolean equals(Object obj)
   {
      if (this == obj)
         return true;
      if (obj == null || (obj instanceof TabularType) == false)
         return false;
      TabularType other = (TabularType) obj;
      if (this.getTypeName().equals(other.getTypeName()) == false)
         return false;
      if (this.getRowType().equals(other.getRowType()) == false)
         return false;
      Iterator thisNames = this.getIndexNames().iterator();
      Iterator otherNames = other.getIndexNames().iterator();
      while(thisNames.hasNext() && otherNames.hasNext())
      {
         String thisName = (String) thisNames.next();
         String otherName = (String) otherNames.next();
         if (thisName.equals(otherName) == false)
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
      cachedHashCode += getRowType().hashCode();
      for (int i = 0; i < indexNames.size(); i++)
         cachedHashCode += indexNames.get(i).hashCode();
      return cachedHashCode;
   }

   public String toString()
   {
      if (cachedToString != null)
         return cachedToString;
      StringBuffer buffer = new StringBuffer(getClass().getName());
      buffer.append(": typeName=[");
      buffer.append(getTypeName());
      buffer.append("] rowType=[");
      buffer.append(getRowType());
      buffer.append("] indexNames=[");
      List thisNames = getIndexNames();
      for (int i = 0; i < thisNames.size(); i++)
      {
         buffer.append(thisNames.get(i));
         if (i + 1 < thisNames.size())
            buffer.append(", ");
      }
      buffer.append("]");
      cachedToString = buffer.toString();
      return cachedToString;
   }
}
