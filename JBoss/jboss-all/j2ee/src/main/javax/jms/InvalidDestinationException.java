/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when a destination is either not understood by a provider or 
  * is no longer valid. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class InvalidDestinationException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a InvalidDestinationException with reason and error code for exception
     */
   public InvalidDestinationException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a InvalidDestinationException with reason and with error code defaulting to null
     */
   public InvalidDestinationException(String reason)
   {
      super(reason,null);
   }

}
