/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * A wrapper for exceptions thrown by MBeans that implement
 * MBeanRegistration. These exceptions are thrown in preRegister and
 * preDeregister.
 *
 * @see javax.management.MBeanRegistration
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class MBeanRegistrationException
   extends MBeanException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new MBeanRegistrationException from a given exception.
    *
    * @param e the exception to wrap.
    */

   public MBeanRegistrationException(Exception e)
   {
      super(e);
   }

   /**
    * Construct a new MBeanRegistrationException from a given exception
    * and message.
    *
    * @param e the exception to wrap.
    * @param message the specified message.
    */
   public MBeanRegistrationException(Exception e, String message)
   {
      super(e, message);
   }

   // Public --------------------------------------------------------

   // MBeanException overrides --------------------------------------

   // Private -------------------------------------------------------
}

