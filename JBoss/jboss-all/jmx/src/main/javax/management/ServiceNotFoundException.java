/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when a service is not supported.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class ServiceNotFoundException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new ServiceNotFoundException with no message.
    */
   public ServiceNotFoundException()
   {
      super();
   }

   /**
    * Construct a new ServiceNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public ServiceNotFoundException(String message)
   {
      super(message);
   }
}

