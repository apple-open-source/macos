/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when an MBean is already registered with the specified ObjectName.
 *
 * @see javax.management.MBeanServer
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class InstanceAlreadyExistsException
   extends OperationsException
{

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InstanceAlreadyExistsException with no message.
    */
   public InstanceAlreadyExistsException()
   {
      super();
   }

   /**
    * Construct a new InstanceAlreadyExistsException with the given message.
    *
    * @param message the error message.
    */
   public InstanceAlreadyExistsException(String message)
   {
      super(message);
   }

   // Public --------------------------------------------------------

   // OperationsException overrides ---------------------------------

   // Private -------------------------------------------------------
}


