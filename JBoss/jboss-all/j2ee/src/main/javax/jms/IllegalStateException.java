/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception is thrown when a method is invoked at an illegal or 
  * inappropriate time or if the provider is not in an appropriate state 
  * for the requested operation. For example, this exception should be 
  * thrown if Session.commit() is called on a non-transacted session. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class IllegalStateException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a IllegalStateException with reason and error code for exception
     */
   public IllegalStateException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a IllegalStateException with reason and with error code defaulting to null
     */
   public IllegalStateException(String reason)
   {
      super(reason,null);
   }

}
