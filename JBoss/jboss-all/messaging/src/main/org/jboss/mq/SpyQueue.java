/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Serializable;
import javax.jms.JMSException;

import javax.jms.Queue;
import javax.naming.Reference;
import javax.naming.Referenceable;
import javax.naming.StringRefAddr;

/**
 *  This class implements javax.jms.Queue
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     David Maplesden (David.Maplesden@orion.co.nz)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class SpyQueue extends SpyDestination implements java.io.Serializable, javax.jms.Queue, javax.naming.Referenceable {
   // Constructor ---------------------------------------------------

   //added cached toString string for efficiency
   private String   toStringStr;

   public SpyQueue( String queueName ) {
      super( queueName );
      toStringStr = "QUEUE." + name;
      hash++;
   }

   public String getQueueName() {
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
            "org.jboss.mq.SpyQueue",
            new StringRefAddr( "name", name ),
            "org.jboss.mq.referenceable.SpyDestinationObjectFactory", null );
   }

   public String toString() {
      return toStringStr;
   }

   // Object override -----------------------------------------------

   //A topic is identified by its name
   public boolean equals( Object obj ) {
      if ( !( obj instanceof SpyQueue ) ) {
         return false;
      }
      if ( obj.hashCode() != hash ) {
         return false;
      }
      return ( ( SpyQueue )obj ).name.equals( name );
   }
}
