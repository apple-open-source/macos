package org.jboss.mq;

/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
import java.io.Serializable;

/**
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.3.2.1 $
 */
public class DurableSubscriptionID implements java.io.Serializable {
   
   String           clientID;
   String           subscriptionName;
   String           selector;

   public DurableSubscriptionID( String id, String subName, String selector ) {
      this.clientID = id;
      this.subscriptionName = subName;
      this.selector = selector;
   }

   /**
    *  Insert the method's description here. Creation date: (7/27/2001 12:40:57
    *  AM)
    *
    * @param  newClientID  java.lang.String
    */
   public void setClientID( java.lang.String newClientID ) {
      clientID = newClientID;
   }

   /**
    *  Insert the method's description here. Creation date: (7/27/2001 12:40:57
    *  AM)
    *
    * @param  newSubscriptionName  java.lang.String
    */
   public void setSubscriptionName( java.lang.String newSubscriptionName ) {
      subscriptionName = newSubscriptionName;
   }

   /**
    *  Insert the method's description here. Creation date: (7/27/2001 12:40:57
    *  AM)
    *
    * @return    java.lang.String
    */
   public java.lang.String getClientID() {
      return clientID;
   }

   /**
    *  Insert the method's description here. Creation date: (7/27/2001 12:40:57
    *  AM)
    *
    * @return    java.lang.String
    */
   public java.lang.String getSubscriptionName() {
      return subscriptionName;
   }

   public boolean equals( Object obj ) {
      try {
         DurableSubscriptionID o = ( DurableSubscriptionID )obj;
         return o.clientID.equals( clientID ) && o.subscriptionName.equals( subscriptionName );
      } catch ( Throwable e ) {
         return false;
      }
   }

   public int hashCode() {
      return Integer.MIN_VALUE + clientID.hashCode() + subscriptionName.hashCode();
   }

   public String toString() {
      return clientID + "." + subscriptionName;
   }
   /**
    * Gets the selector.
    * @return Returns a String
    */
   public String getSelector()
   {
      if (selector == null || selector.trim().length() == 0)
         return null;
      return selector;
   }

   /**
    * Sets the selector.
    * @param selector The selector to set
    */
   public void setSelector(String selector)
   {
      this.selector = selector;
   }

}
