/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when a string used to construct an ObjectName is not valid.
 *
 * @see javax.management.ObjectName
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class MalformedObjectNameException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new MalformedObjectNameException with no message.
    */
   public MalformedObjectNameException()
   {
      super();
   }

   /**
    * Construct a new MalformedObjectNameException with the given message.
    *
    * @param message the error message.
    */
   public MalformedObjectNameException(String message)
   {
      super(message);
   }
}

