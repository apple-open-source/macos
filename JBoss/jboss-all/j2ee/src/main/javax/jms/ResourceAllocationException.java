/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package javax.jms;

/**
  * This exception is thrown when a provider is unable to allocate the resources 
  * required by a method. For example, this exception should be throw when a 
  * call to createTopicConnection fails due to lack of JMS Provider resources. 
  *
  * @author Chris Kimpton (chris@kimptoc.net)
  * @version $Revision: 1.1 $
 **/
public class ResourceAllocationException extends JMSException
{
   // CONSTRUCTORS -----------------------------------------------------

   /** 
     * Construct a ResourceAllocationException with reason and error code for exception
     */
   public ResourceAllocationException(String reason, String errorCode)
   {
      super(reason,errorCode);
   }

   /** 
     * Construct a ResourceAllocationException with reason and with error code defaulting to null
     */
   public ResourceAllocationException(String reason)
   {
      super(reason,null);
   }

}
