/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.util.Collection;
import java.util.Set;

/**
 * An Open Data Type for tabular data structures.<p>
 *
 * @see TabularDataSupport
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public interface TabularData
{

   // Attributes ----------------------------------------------------

   // Public --------------------------------------------------------

   /**
    * Retrieve the tabular type for this tabular data
    *
    * @return the tabular type
    */
   TabularType getTabularType();

   /**
    * Calculate the index for the value passed if it were added to the
    * tabular data. The validity of the passed value is checked. But the
    * tabular data isn't checked to see whether the index is already used.
    *
    * @param value the value for which the index is calculated.
    * @return the calculated index
    * @exception NullPointerException for a null value
    * @exception InvalidOpenTypeException when the passed value is not
    *            valid for the tabular data's row type.
    */
   Object[] calculateIndex(CompositeData value);

   /**
    * Retrieve the number of rows in the tabular data.
    *
    * @return the number of rows.
    */
   int size();

   /**
    * Determine whether the tabular data is empty.
    *
    * @return true when there are no rows, false otherwise
    */
   boolean isEmpty();

   /**
    * Determine whether the tabular data contains the passed value as a row.
    * If the passed value is null or invalid, false is returned.
    *
    * @param key the value to check
    * @return true when the value is a row index, false otherwise
    */
   boolean containsKey(Object[] key);

   /**
    * Determine whether the tabular data contains the passed value.
    * If the passed value is null or invalid, false is returned.
    *
    * @param value the value to check
    * @return true when the value is a row index, false otherwise
    */
   boolean containsValue(CompositeData value);

   /**
    * Retrieve the composite data for the passed index.
    *
    * @param key the index to retrieve
    * @exception NullPointerException when the passed key is null
    * @exception InvalidKeyExcception when the passed key does match
    *            the row type of the tabular data.
    */
   CompositeData get(Object[] key);

   /**
    * Add a value to the tabular data. The value must have the same
    * CompositeType has the tabular data and there is no value already
    * occupying the index for the value.
    *
    * @param value the value to add
    * @exception NullPointerException when the passed value is null
    * @exception InvalidOpenTypeException when the value is not valid for
    *            the row type of the tabular data
    * @exception KeyAlreadyExistsException when the index for the value
    *            is already occupied.
    */
   void put(CompositeData value);

   /**
    * Removes the value for the passed and returns the removed value, or
    * null if the key was not present.
    *
    * @param key the index of the value to remove
    * @exception NullPointerException when the passed key is null
    * @exception InvalidKeyExecption when the key is not valid for the 
    *            tabular data
    */
   CompositeData remove(Object[] key);
      
   /**
    * Add all the passed values. All the values are checked before
    * addition including any duplicates that might be added. Either all
    * or no value is added.
    *
    * @param values the values to add
    * @exception NullPointerException when the passed values is null or
    *            an element of the values is null
    * @exception InvalidOpenTypeException when one of value is not valid for
    *            the row type of the tabular data
    * @exception KeyAlreadyExistsException when the index for one of the values
    *            is already occupied.
    */
   void putAll(CompositeData[] values);
      
   /**
    * Removes all CompositeData values from the Tabular Data
    */
   void clear();
      
   /**
    * Returns a set view of the index values.
    *
    * @return the set of index values.
    */
   Set keySet();
      
   /**
    * Returns a set view of the row values.
    *
    * @return the set of row values.
    */
   Collection values();

   /**
    * Tests whether two tabular data objects are equal<p>
    *
    * The object is non-null<br>
    * The object implements this interface<br>
    * The row types are equal<br>
    * The index to value mappings are equal
    *
    * @param obj the object to test
    * @return true when the above conditions are satisfied, false otherwise.
    */
   boolean equals(Object obj);

   /**
    * Generates a hashcode for the implementation.<p>
    *
    * The sum of the hashCodes for the elements mentioned in the equals
    * method
    *
    * @return the calculated hashcode
    */
   int hashCode();

   /**
    * A string representation of the open mbean operation info.<p>
    *
    * It is made up of implementation class and the values mentioned
    * in the equals method
    *
    * @return the string
    */
   String toString();
}
