/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import javax.jms.IllegalStateException;
import javax.jms.InvalidSelectorException;
import javax.jms.JMSException;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.Queue;

import javax.jms.QueueReceiver;

/**
 *  This class implements javax.jms.QueueReceiver
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.3 $
 */
public class SpyQueueReceiver extends SpyMessageConsumer implements QueueReceiver {
   // Attributes ----------------------------------------------------

   //The queue I registered
   private Queue    queue;

   // Constructor ---------------------------------------------------

   SpyQueueReceiver( SpyQueueSession session, Queue queue, String selector ) 
   	throws InvalidSelectorException {
      super( session, false );
      this.queue = queue;

      subscription.destination = ( SpyDestination )queue;
      subscription.messageSelector = selector;
      subscription.noLocal = false;
      
      // If the selector is set, try to build it, throws an InvalidSelectorException 
      // if it is not valid.
      if( subscription.messageSelector!=null )
      	subscription.getSelector();
   }

   // Public --------------------------------------------------------

   public Queue getQueue()
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }

      return queue;
   }

}
