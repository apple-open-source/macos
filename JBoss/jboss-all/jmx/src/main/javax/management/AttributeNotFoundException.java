/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when the attribute does not exist or cannot be retrieved.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.3 $
 */
public class AttributeNotFoundException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new AttributeNotFoundException with no message.
    */
   public AttributeNotFoundException()
   {
      super();
   }

   /**
    * Construct a new AttributeNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public AttributeNotFoundException(String message)
   {
      super(message);
   }

   // Public --------------------------------------------------------

   // OperationsException overrides ---------------------------------

   // Private -------------------------------------------------------
}
