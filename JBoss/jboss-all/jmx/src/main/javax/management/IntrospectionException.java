/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when an error occurs introspecting an MBean.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.3 $
 */
public class IntrospectionException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new IntrospectionException with no message.
    */
   public IntrospectionException()
   {
      super();
   }

   /**
    * Construct a new IntrospectionException with the given message.
    *
    * @param message the error message.
    */
   public IntrospectionException(String message)
   {
      super(message);
   }
}

