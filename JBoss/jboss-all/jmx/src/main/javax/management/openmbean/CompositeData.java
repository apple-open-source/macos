/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.util.Collection;

/**
 * An Open Data Type for composite data structures.<p>
 *
 * @see CompositeDataSupport
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.2.2.1 $
 *
 */
public interface CompositeData
{

   // Attributes ----------------------------------------------------

   // Public --------------------------------------------------------

   /**
    * Retrieve the composite type for this composite data
    *
    * @return the composite type
    */
   CompositeType getCompositeType();

   /**
    * Retrieve the value for the item with the passed key
    *
    * @param the key to the item
    * @return the value
    * @exception IllegalArgumentException when the key is null or the empty
    *             string
    * @exception InvalidKeyException when the key does not exist 
    */
   Object get(String key);

   /**
    * Retrieve the arrray of values for the item with the passed keys
    *
    * @param an array of key values
    * @return the array of values
    * @exception IllegalArgumentException when a key is null or the empty
    *             string or the array is null
    * @exception InvalidKeyException when a key does not exist 
    */
   Object[] getAll(String[] key);

   /**
    * Tests whether a key is part of this composite data
    *
    * @param the key to test
    * @return true when the key exists, false otherwise
    */
   boolean containsKey(String key);

   /**
    * Tests whether a item exists with the passed value
    *
    * @param the value to test
    * @return true when the value exists, false otherwise
    */
   boolean containsValue(Object value);

   /**
    * The values of this composite data<p>
    *
    * An iterator over the returned collection returns result in ascending
    * lexicographic order
    *
    * @return an unmodifiable Collection of the values of this CompositeType.
    */
   Collection values();

   /**
    * Tests whether two composite data objects are equal<p>
    *
    * The object is non-null<br>
    * The object implements this interface<br>
    * The composite types are equal<br>
    * The values are equal
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
