/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception is thrown when an operation is invalid because a transaction 
  * is in progress. For instance, attempting to call Session.commit() when a 
  * session is part of a distributed transaction should throw a TransactionInProgressException. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class TransactionInProgressException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a TransactionInProgressException with reason and error code for exception
     */
   public TransactionInProgressException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a TransactionInProgressException with reason and with error code defaulting to null
     */
   public TransactionInProgressException(String reason)
   {
      super(reason,null);
   }

}
