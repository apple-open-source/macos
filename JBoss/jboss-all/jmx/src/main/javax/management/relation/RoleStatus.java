/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * The problems that occur when resolving roles.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class RoleStatus
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   /**
    * Tried to set a role with less objects that minimum cardinality
    */
   public static int LESS_THAN_MIN_ROLE_DEGREE = 1;

   /**
    * Tried to set a role with more objects that maximum cardinality
    */
   public static int MORE_THAN_MAX_ROLE_DEGREE = 2;

   /**
    * Tried to use an unknown role
    */
   public static int NO_ROLE_WITH_NAME = 3;

   /**
    * Tried to use an an object name that is not registered
    */
   public static int REF_MBEAN_NOT_REGISTERED = 4;

   /**
    * Tried to use an an object name for an MBean with an incorrect class
    */
   public static int REF_MBEAN_OF_INCORRECT_CLASS = 5;

   /**
    * Tried to access a role that is not readable
    */
   public static int ROLE_NOT_READABLE = 6;

   /**
    * Tried to set a role that is not writable
    */
   public static int ROLE_NOT_WRITABLE = 7;

   /**
    * See if the passed integer is a valid problem type.
    * 
    * @return true when it is, false otherwise.
    */
   public static boolean isRoleStatus(int problemType)
   {
     return (problemType >= LESS_THAN_MIN_ROLE_DEGREE && problemType <= ROLE_NOT_WRITABLE);
   }

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------
}

