/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import javax.management.MBeanParameterInfo;

/**
 * An open MBean operation info implements this interface as well as extending
 * MBeanOperationInfo.<p>
 * 
 * {@link OpenMBeanOperationInfoSupport} is an example of such a class.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public interface OpenMBeanOperationInfo
{
   // Attributes ----------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   /**
    * Retrieve a human readable description of the open MBean operation the
    * implementation of this interface describes.
    *
    * @return the description.
    */
   String getDescription();

   /**
    * Retrieve the name of the operation described.
    *
    * @return the name.
    */
   String getName();

   /**
    * Returns an array of the parameters passed to the operation<p>
    *
    * The parameters must be OpenMBeanParameterInfos.
    *
    * @return the operation's parameters.
    */
   MBeanParameterInfo[] getSignature();

   /**
    * Retrieves the impact of the operation.<pr>
    *
    * One of<br>
    * {@link javax.management.MBeanOperationInfo#INFO}<br>
    * {@link javax.management.MBeanOperationInfo#ACTION}<br>
    * {@link javax.management.MBeanOperationInfo#ACTION_INFO}
    *
    * @return the impact.
    */
   int getImpact();

   /**
    * Retrieves the return type of operation.<pr>
    *
    * This must be same as getReturnOpenType().getClassName()
    *
    * @return the return type.
    */
   String getReturnType();

   /**
    * Retrieves the open type return type of operation.
    *
    * @return the open type of the return type.
    */
   OpenType getReturnOpenType();

   /**
    * Compares an object for equality with the implementing class.<p>
    *
    * The object is not null<br>
    * The object implements the open mbean operation info interface<br>
    * The operation names are equal<br>
    * The signatures are equal<br>
    * The return types are equal<br>
    * The impacts are equal<br>
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
    * A string representation of the open mbean operation info.<p>
    *
    * It is made up of implementation class and the values mentioned
    * in the equals method
    *
    * @return the string
    */
   String toString();
}
