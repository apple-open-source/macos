/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception must be thrown when a provider rejects a user name/password submitted 
  * by a client. It may also be thrown for any case where a security restriction prevents 
  * a method from completing. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class JMSSecurityException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a JMSSecurityException with reason and error code for exception
     */
   public JMSSecurityException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a JMSSecurityException with reason and with error code defaulting to null
     */
   public JMSSecurityException(String reason)
   {
      super(reason,null);
   }

}
