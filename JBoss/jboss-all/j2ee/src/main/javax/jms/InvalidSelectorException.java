/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when a JMS client attempts to give a provider a message 
  * selector with invalid syntax. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class InvalidSelectorException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a InvalidSelectorException with reason and error code for exception
     */
   public InvalidSelectorException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a InvalidSelectorException with reason and with error code defaulting to null
     */
   public InvalidSelectorException(String reason)
   {
      super(reason,null);
   }

}
