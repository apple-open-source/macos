/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when a role info has a minimum greater than its maximum.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class InvalidRoleInfoException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InvalidRoleInfoException with no message.
    */
   public InvalidRoleInfoException()
   {
      super();
   }

   /**
    * Construct a new InvalidRoleInfoException with the given message.
    *
    * @param message the error message.
    */
   public InvalidRoleInfoException(String message)
   {
      super(message);
   }
}

