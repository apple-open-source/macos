/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when the a role info with the given name doesn't exist for
 * relation type.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class RoleInfoNotFoundException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RoleInfoNotFoundException with no message.
    */
   public RoleInfoNotFoundException()
   {
      super();
   }

   /**
    * Construct a new RoleInfoNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public RoleInfoNotFoundException(String message)
   {
      super(message);
   }
}

