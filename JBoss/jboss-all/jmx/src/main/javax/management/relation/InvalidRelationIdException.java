/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when a relation id provided has already been used.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class InvalidRelationIdException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InvalidRelationIdException with no message.
    */
   public InvalidRelationIdException()
   {
      super();
   }

   /**
    * Construct a new InvalidRelationIdException with the given message.
    *
    * @param message the error message.
    */
   public InvalidRelationIdException(String message)
   {
      super(message);
   }
}

