/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when trying to register an MBean that does not conform the
 * JMX specification.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class NotCompliantMBeanException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new NotCompliantMBeanException with no message.
    */
   public NotCompliantMBeanException()
   {

      super();
   }

   /**
    * Construct a new NotCompliantMBeanException with the given message.
    *
    * @param message the error message.
    */
   public NotCompliantMBeanException(String message)
   {
      super(message);

   }
}

