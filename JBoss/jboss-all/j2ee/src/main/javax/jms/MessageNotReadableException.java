/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when a JMS client attempts to read a write-only message. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class MessageNotReadableException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a MessageNotReadableException with reason and error code for exception
     */
   public MessageNotReadableException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a MessageNotReadableException with reason and with error code defaulting to null
     */
   public MessageNotReadableException(String reason)
   {
      super(reason,null);
   }

}
