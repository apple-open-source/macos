/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when the number of MBeans passed is less the minimum or greater
 * than the maximum degree of a role, or the an MBean has an incorrect
 * class or an MBean does not exist.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class InvalidRoleValueException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new InvalidRoleValueException with no message.
    */
   public InvalidRoleValueException()
   {
      super();
   }

   /**
    * Construct a new InvalidRoleValueException with the given message.
    *
    * @param message the error message.
    */
   public InvalidRoleValueException(String message)
   {
      super(message);
   }
}

