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
import javax.jms.Topic;

import javax.jms.TopicSubscriber;

/**
 * This class implements <tt>javax.jms.TopicSubscriber</tt>.
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.4 $
 */
public class SpyTopicSubscriber
       extends SpyMessageConsumer
       implements TopicSubscriber
{
   // Attributes ----------------------------------------------------

   /** The registered topic. */
   private SpyTopic topic;


   // Constructor ---------------------------------------------------

   SpyTopicSubscriber( SpyTopicSession session, SpyTopic topic, boolean noLocal, String selector ) 
   	throws InvalidSelectorException {
      super( session, false );
      this.topic = topic;

      subscription.destination = ( SpyDestination )topic;
      subscription.messageSelector = selector;
      subscription.noLocal = noLocal;
      
      // If the selector is set, try to build it, throws an InvalidSelectorException 
      // if it is not valid.
      if( subscription.messageSelector!=null )
      	subscription.getSelector();
      
   }

   // Public --------------------------------------------------------

   public Topic getTopic()
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      return topic;
   }

   public boolean getNoLocal()
      throws JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The MessageConsumer is closed" );
      }
      return subscription.noLocal;
   }
}
