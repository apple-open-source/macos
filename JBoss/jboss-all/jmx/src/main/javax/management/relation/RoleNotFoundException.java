/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when the a role does not exist for a relation or is not
 * readable or writable for the respective operation.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class RoleNotFoundException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RoleNotFoundException with no message.
    */
   public RoleNotFoundException()
   {
      super();
   }

   /**
    * Construct a new RoleNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public RoleNotFoundException(String message)
   {
      super(message);
   }
}

