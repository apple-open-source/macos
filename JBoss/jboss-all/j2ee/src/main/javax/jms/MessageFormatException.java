/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when a JMS client attempts to use a data type not 
  * supported by a message or attempts to read data in a message as the wrong type. 
  * It must also be thrown when equivalent type errors are made with message property 
  * values. For example, this exception must be thrown if StreamMessage.setObject() 
  * is given an unsupported class or if StreamMessage.getShort() is used to read a 
  * boolean value. Note that the special case of a failure caused by attempting to 
  * read improperly formatted String data as numeric values should throw the 
  * java.lang.NumberFormatException. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class MessageFormatException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a MessageFormatException with reason and error code for exception
     */
   public MessageFormatException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a MessageFormatException with reason and with error code defaulting to null
     */
   public MessageFormatException(String reason)
   {
      super(reason,null);
   }

}
