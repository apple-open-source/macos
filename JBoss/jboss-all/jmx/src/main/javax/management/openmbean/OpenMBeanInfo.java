/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.openmbean;

import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanNotificationInfo;

/**
 * An MBean is an OpenMBean when its getMBeanInfo() returns an MBeanInfo
 * implementing this interface and extending javax.management.MBeanInfo.<p>
 * 
 * {@link OpenMBeanInfoSupport} is an example of such a class.<p>
 * 
 * The MBean info classes should be the OpenInfo versions. e.g.
 * an {@link OpenMBeanAttributeInfo} instead of MBeanAttributeInfo.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.1.2.1 $
 *
 */
public interface OpenMBeanInfo
{

   // Attributes ----------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

   /**
    * Retrieve the fully qualified class name of the open MBean the
    * implementation of this interface describes.
    *
    * @return the fully qualified class name.
    */
   String getClassName();

   /**
    * Retrieve a human readable description of the open MBean the
    * implementation of this interface describes.
    *
    * @return the description.
    */
   String getDescription();

   /**
    * Retrieve an array of OpenMBeanAttributeInfos describing each attribute
    * of the open mbean.<p>
    *
    * Each instance should also implement OpenMBeanAttributeInfo
    *
    * @return the array of attributes.
    */
   MBeanAttributeInfo[] getAttributes();

   /**
    * Retrieve an array of OpenMBeanConstructorInfos describing each constructor
    * of the open mbean.<p>
    *
    * Each instance should also implement OpenMBeanConstructorInfo
    *
    * @return the array of constructors.
    */
   MBeanConstructorInfo[] getConstructors();

   /**
    * Retrieve an array of OpenMBeanOperationInfos describing each operation
    * of the open mbean.<p>
    *
    * Each instance should also implement OpenMBeanOperationInfo
    *
    * @return the array of operations.
    */
   MBeanOperationInfo[] getOperations();

   /**
    * Retrieve an array of MBeanNotificationInfos describing each notification
    * of the open mbean.
    *
    * @return the array of notifications.
    */
   MBeanNotificationInfo[] getNotifications();

   /**
    * Compares an object for equality with the implementing class.<p>
    *
    * The object is not null<br>
    * The object implements the open mbean info interface<br>
    * The getClassName() methods return strings that are equal<br>
    * The information objects (attributes, constructors, operations and
    * notifications) are the equal
    *
    * @param obj the object to test
    * @return true when above is true, false otherwise
    */
   boolean equals(Object obj);

   /**
    * Generates a hashcode for the implementation.<p>
    *
    * The hashcode is the sum of the hashcodes for<br>
    * getClassName()<br>
    * java.util.HashSet(java.util.Arrays.asList(getAttributes()).hashCode()<br>
    * java.util.HashSet(java.util.Arrays.asList(getConstructors()).hashCode()<br>
    * java.util.HashSet(java.util.Arrays.asList(getOperations()).hashCode()<br>
    * java.util.HashSet(java.util.Arrays.asList(getNotifications()).hashCode()<br>
    *
    * @return the calculated hashcode
    */
   int hashCode();

   /**
    * A string representation of the open mbean info.<p>
    *
    * It is made up of<br>
    * The implementing class<br>
    * getClassName()<br>
    * toString() for each of the info arrays
    *
    * @return the string
    */
   String toString();
}
