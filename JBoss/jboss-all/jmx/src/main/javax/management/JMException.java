/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Exceptions thrown by JMX implementations. These types of errors
 * are due to incorrect invocations as opposed to JMRuntimeExceptions
 * which are errors during the invocation.
 *
 * @see javax.management.JMRuntimeException
 *
 * @author <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.3 $
 */
public class JMException
   extends Exception
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new JMException with no message.
    */
   public JMException()
   {
      super();
   }

   /**
    * Construct a new JMException with the given message.
    *
    * @param message the error message.
    */
   public JMException(String message)
   {
      super(message);
   }

   // Public --------------------------------------------------------

   // Exception overrides -------------------------------------------

   // Private -------------------------------------------------------
}
