/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when a relation type is not registered with the relation service.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class RelationTypeNotFoundException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RelationTypeNotFoundException with no message.
    */
   public RelationTypeNotFoundException()
   {
      super();
   }

   /**
    * Construct a new RelationTypeNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public RelationTypeNotFoundException(String message)
   {
      super(message);
   }
}

