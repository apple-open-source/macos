/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

/**
 * Thrown when a specified Listener does not exist.
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.2 $
 */
public class ListenerNotFoundException
   extends OperationsException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new ListenerNotFoundException with no message.
    */
   public ListenerNotFoundException()
   {
      super();
   }

   /**
    * Construct a new ListenerNotFoundException with the given message.
    *
    * @param message the error message.
    */
   public ListenerNotFoundException(java.lang.String message)
   {
      super(message);
   }

   // Public --------------------------------------------------------

   // OperationsException overrides ---------------------------------

   // Private -------------------------------------------------------
}
