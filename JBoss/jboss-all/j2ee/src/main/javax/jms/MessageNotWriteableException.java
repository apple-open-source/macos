/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when a JMS client attempts to write to a read-only message
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class MessageNotWriteableException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a MessageNotWriteableException with reason and error code for exception
     */
   public MessageNotWriteableException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a MessageNotWriteableException with reason and with error code defaulting to null
     */
   public MessageNotWriteableException(String reason)
   {
      super(reason,null);
   }

}
