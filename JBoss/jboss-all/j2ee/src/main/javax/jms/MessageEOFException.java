/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when an unexpected end of stream has been reached when a 
  * StreamMessage or BytesMessage is being read. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class MessageEOFException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a MessageEOFException with reason and error code for exception
     */
   public MessageEOFException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a MessageEOFException with reason and with error code defaulting to null
     */
   public MessageEOFException(String reason)
   {
      super(reason,null);
   }

}
