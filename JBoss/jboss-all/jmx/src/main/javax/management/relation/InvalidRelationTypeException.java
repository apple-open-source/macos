/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when the relation type already exists with the given name or
 * the same name is used in different role infos or there is no role info or
 * a null role info.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class InvalidRelationTypeException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InvalidRelationTypeException with no message.
    */
   public InvalidRelationTypeException()
   {
      super();
   }

   /**
    * Construct a new InvalidRelationTypeException with the given message.
    *
    * @param message the error message.
    */
   public InvalidRelationTypeException(String message)
   {
      super(message);
   }
}

