/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import javax.management.MBeanParameterInfo;

/**
 * An open MBean constructor info implements this interface as well as extending
 * MBeanConstructorInfo.<p>
 * 
 * {@link OpenMBeanConstructorInfoSupport} is an example of such a class.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public interface OpenMBeanConstructorInfo
{
   // Attributes ----------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   /**
    * Retrieve a human readable description of the open MBean constructor the
    * implementation of this interface describes.
    *
    * @return the description.
    */
   String getDescription();

   /**
    * Retrieve the name of the constructor described.
    *
    * @return the name.
    */
   String getName();

   /**
    * Returns an array of the parameters passed to the constructor<p>
    *
    * The parameters must be OpenMBeanParameterInfos.
    *
    * @return the constructor's parameters.
    */
   MBeanParameterInfo[] getSignature();

   /**
    * Compares an object for equality with the implementing class.<p>
    *
    * The object is not null<br>
    * The object implements the open mbean constructor info interface<br>
    * The constructor names are equal<br>
    * The signatures are equal<br>
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
    * A string representation of the open mbean constructor info.<p>
    *
    * It is made up of implementation class and the values mentioned
    * in the equals method
    *
    * @return the string
    */
   String toString();
}
