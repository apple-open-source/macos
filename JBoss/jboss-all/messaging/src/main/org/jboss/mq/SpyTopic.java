/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Serializable;
import javax.jms.JMSException;

import javax.jms.Topic;
import javax.naming.Reference;
import javax.naming.Referenceable;
import javax.naming.StringRefAddr;

/**
 *  This class implements javax.jms.Topic
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.4 $
 */
public class SpyTopic extends SpyDestination implements java.io.Serializable, javax.jms.Topic, javax.naming.Referenceable {

   protected DurableSubscriptionID durableSubscriptionID;
   // Constructor ---------------------------------------------------

   //added cached toString string for efficiency
   private String   toStringStr;

   public SpyTopic( String topicName ) {
      super( topicName );
      toStringStr = "TOPIC." + name;
   }

   // Constructor ---------------------------------------------------

   public SpyTopic( SpyTopic topic, String clientID, String subscriptionName, String selector ) {
      this( topic, new DurableSubscriptionID( clientID, subscriptionName, selector ) );
   }

   // Constructor ---------------------------------------------------

   public SpyTopic( SpyTopic topic, DurableSubscriptionID subid ) {
      super( topic.getTopicName() );
      if ( subid == null ) {
         toStringStr = "TOPIC." + name;
      } else {
         toStringStr = "TOPIC." + name + "." + subid;
      }
      this.durableSubscriptionID = subid;
   }

   // Public --------------------------------------------------------

   public String getTopicName() {
      return name;
   }

   /**
    *  getReference method - to implement javax.naming.Refrenceable
    *
    * @return                                   The Reference value
    * @exception  javax.naming.NamingException  Description of Exception
    */
   public Reference getReference()
      throws javax.naming.NamingException {
      return new Reference(
            "org.jboss.mq.SpyTopic",
            new StringRefAddr( "name", name ),
            "org.jboss.mq.referenceable.SpyDestinationObjectFactory", null );
   }

   public DurableSubscriptionID getDurableSubscriptionID() {
      return durableSubscriptionID;
   }

   public String toString() {
//		if( durableSubscriptionID != null)
//			return toStringStr+"."+durableSubscriptionID;
      return toStringStr;
   }

   // Object override -----------------------------------------------

   public boolean equals( Object obj ) {
      if ( !( obj instanceof SpyTopic ) ) {
         return false;
      }
      if ( obj.hashCode() != hash ) {
         return false;
      }
      return ( ( SpyDestination )obj ).name.equals( name );
   }
}
