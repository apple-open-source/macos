/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when an error occurs performing an operation on an MBean.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class OperationsException
   extends JMException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new OperationsException with no message.
    */
   public OperationsException()
   {
      super();
   }

   /**
    * Construct a new OperationsException with the given message.
    *
    * @param message the error message.
    */
   public OperationsException(String message)
   {
      super(message);
   }
}

