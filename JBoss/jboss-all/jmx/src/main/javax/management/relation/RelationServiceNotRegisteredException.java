/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * Thrown when the relation service has not been registered with the MBean Server.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class RelationServiceNotRegisteredException
   extends RelationException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new RelationServiceNotRegisteredException with no message.
    */
   public RelationServiceNotRegisteredException()
   {
      super();
   }

   /**
    * Construct a new RelationServiceNotRegisteredException with the given message.
    *
    * @param message the error message.
    */
   public RelationServiceNotRegisteredException(String message)
   {
      super(message);
   }
}

