/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when a client attempts to set a 
  * connection's client id to a value that is rejected by a provider. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class InvalidClientIDException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a InvalidClientIDException with reason and error code for exception
     */
   public InvalidClientIDException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a InvalidClientIDException with reason and with error code defaulting to null
     */
   public InvalidClientIDException(String reason)
   {
      super(reason,null);
   }

}
