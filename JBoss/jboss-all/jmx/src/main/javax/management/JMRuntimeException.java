/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Exceptions thrown by JMX implementations. These types of errors
 * occur during invocations as opposed to JMExceptions
 * which are due to incorrect invocations.
 *
 * @see javax.management.JMException
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class JMRuntimeException
   extends RuntimeException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new JMRuntimeException with no message.
    */
   public JMRuntimeException()
   {
      super();
   }

   /**
    * Construct a new JMRuntimeException with the given message.
    *
    * @param message the error message.
    */
   public JMRuntimeException(String message)
   {
      super(message);
   }

   // Public --------------------------------------------------------

   // RuntimeException overrides ------------------------------------

   // Private -------------------------------------------------------
}

