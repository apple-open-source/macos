/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when an MBean is not registered with the specified ObjectName.
 *
 * @see javax.management.MBeanServer
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class InstanceNotFoundException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InstanceNotFoundException with no message.
    */
   public InstanceNotFoundException()
   {
      super();
   }

   /**
    * Construct a new InstanceNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public InstanceNotFoundException(String message)
   {
      super(message);
   }

}

