/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This is the root class of all JMS exceptions. 
  * 
  * <p>It provides following information: 
  * <ul>
  *   <li>provider-specific string describing the error -
  *       This string is the standard Java exception message,
  *       and is available via getMessage(). 
  *   <li>provider-specific, string error code 
  *   <li>reference to another exception - Often a JMS exception will be
  *       the result of a lower level problem. If appropriate, this lower
  *       level exception can be linked to the JMS exception. 
  * </ul>
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
  * @version $Revision: 1.3 $
 **/
public class JMSException
   extends Exception
{
   /** The specified error code */
   private String errorCode; // = null;

   /** A linked exception */
   private Exception linkedException;
   
   /** 
    * Construct a JMSException with reason and error code for exception
    */
   public JMSException(String reason, String errorCode)
   {
      super(reason);
      this.errorCode = errorCode;
   }

   /** 
    * Construct a JMSException with reason and with error code defaulting
    * to null.
    */
   public JMSException(String reason)
   {
      this(reason, null);
   }

   /**
    * Get the vendor specific error code.
    */
   public String getErrorCode()
   {
      return errorCode;
   }

   /**
    * Get the exception linked to this one.  
    */
   public Exception getLinkedException()
   {
      return linkedException;
   }

   /**
    * Set the linked exception
    */
   public synchronized void setLinkedException(Exception ex)
   {
      linkedException = ex;
   }
}
