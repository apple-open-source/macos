/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when an invalid relation service is used.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class InvalidRelationServiceException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InvalidRelationServiceException with no message.
    */
   public InvalidRelationServiceException()
   {
      super();
   }

   /**
    * Construct a new InvalidRelationServiceException with the given message.
    *
    * @param message the error message.
    */
   public InvalidRelationServiceException(String message)
   {
      super(message);
   }
}

