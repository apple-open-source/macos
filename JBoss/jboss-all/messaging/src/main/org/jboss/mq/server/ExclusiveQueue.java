package org.jboss.mq.server;

import javax.jms.JMSException;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.Subscription;

/**
 *  This class implements a basic queue with an exclusive subscription.
 *
 * @author     Adrian Brock (Adrian.Brock@HappeningTimes.com)
 * @created    28th October 2002
 */
public class ExclusiveQueue
   extends BasicQueue
{
   Subscription exclusive;
   boolean removed = false;

   public ExclusiveQueue(JMSDestinationManager server, SpyDestination destination,
                         Subscription exclusive)
      throws JMSException
   {
      super(server, destination.toString() + "." + exclusive.connectionToken.getClientID() 
                                           + '.' + exclusive.subscriptionId);
      this.exclusive = exclusive;
   }

   public Subscription getExclusiveSubscription()
   {
      return exclusive;
   }

   public void addMessage(MessageReference mesRef, org.jboss.mq.pm.Tx txId)
      throws JMSException
   {
      // Ignore the message if we are not interested
      if (removed || exclusive.accepts(mesRef.getHeaders()) == false)
         dropMessage(mesRef);
      else
         super.addMessage( mesRef, txId );
   }

   public void restoreMessage(MessageReference mesRef)
   {
      if (removed)
         dropMessage(mesRef);
      else
         super.restoreMessage(mesRef);
   }

   public void removeSubscriber(Subscription sub)
   {
      removed = true;
      super.removeSubscriber(sub);
   }
}
