/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when there is no relation for a passed relation id.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class RelationNotFoundException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RelationNotFoundException with no message.
    */
   public RelationNotFoundException()
   {
      super();
   }

   /**
    * Construct a new RelationNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public RelationNotFoundException(String message)
   {
      super(message);
   }
}

