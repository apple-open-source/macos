/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;


/**
 * Thrown when trying to change an attribute to a incorrect value or type.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class InvalidAttributeValueException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InvalidAttributeValueException with no message.
    */
   public InvalidAttributeValueException()
   {
      super();
   }

   /**
    * Construct a new InvalidAttributeValueException with the given message.
    *
    * @param message the error message.
    */
   public InvalidAttributeValueException(String message)
   {
      super(message);
   }

   // Public --------------------------------------------------------

   // OperationsException overrides ---------------------------------

   // Private -------------------------------------------------------
}

