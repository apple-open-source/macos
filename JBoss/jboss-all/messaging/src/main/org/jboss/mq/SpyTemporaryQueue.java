/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import javax.jms.JMSException;

import javax.jms.TemporaryQueue;

import org.jboss.logging.Logger;

/**
 *  This class implements javax.jms.TemporaryQueue
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.3.4.1 $
 */
public class SpyTemporaryQueue
       extends SpyQueue
       implements TemporaryQueue {

   //The DistributedConnection of its creator
   ConnectionToken  dc;

   // Connection to the creator used from the client side
   private transient Connection con = null;

   static Logger cat = Logger.getLogger( SpyTemporaryQueue.class );

   // Constructor ---------------------------------------------------

   public SpyTemporaryQueue( String queueName, ConnectionToken dc_ ) {
      super( queueName );
      dc = dc_;
   }


   // Public --------------------------------------------------------

   /**
    * Client-side temporary queues need a reference to the connection
    * that created them in case delete() is called.
    */
   public void setConnection( Connection con )
   {
       this.con = con;
   }

   public void delete()
      throws JMSException {
      try {
         con.deleteTemporaryDestination( this );
      } catch ( Exception e ) {
         throw new SpyJMSException( "Cannot delete the TemporaryQueue", e );
      }
   }
}
