/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import java.util.Set;

/**
 * An open MBean attribute info implements this interface as well as extending
 * MBeanAttributeInfo.<p>
 * 
 * {@link OpenMBeanAttributeInfoSupport} is an example of such a class.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public interface OpenMBeanAttributeInfo
   extends OpenMBeanParameterInfo
{
   // Attributes ----------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   /**
    * Test whether an attribute is readable.
    *
    * @return true when the attribute is readable or false otherwise
    */
   boolean isReadable();

   /**
    * Test whether an attribute is writable.
    *
    * @return true when the attribute is writable or false otherwise
    */
   boolean isWritable();

   /**
    * Test whether an attribute is accessed through an isXXX getter.
    *
    * @return the result of the above test
    */
   boolean isIs();

   /**
    * Compares an object for equality with the implementing class.<p>
    *
    * The object is not null<br>
    * The object implements the open mbean parameter info interface<br>
    * The parameter names are equal<br>
    * The open types are equal<br>
    * The access properties are the same<br>
    * The default, min, max and legal values are equal
    *
    * @param obj the object to test
    * @return true when above is true, false otherwise
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
    * A string representation of the open mbean parameter info.<p>
    *
    * It is made up of implementation class and the values mentioned
    * in the equals method
    *
    * @return the string
    */
   String toString();
}
