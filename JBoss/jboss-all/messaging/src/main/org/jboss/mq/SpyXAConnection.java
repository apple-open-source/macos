/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;

import java.io.Serializable;

import javax.jms.JMSException;
import javax.jms.QueueSession;

import javax.jms.IllegalStateException;
import javax.jms.TopicSession;
import javax.jms.XAQueueConnection;
import javax.jms.XAQueueSession;
import javax.jms.XATopicConnection;

import javax.jms.XATopicSession;

import org.jboss.mq.il.ServerIL;

/**
 *  This class implements javax.jms.XAQueueConnection
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.4 $
 */
public class SpyXAConnection
       extends SpyConnection
       implements Serializable, XATopicConnection, XAQueueConnection {

   // Constructor ---------------------------------------------------
   public SpyXAConnection( String userid, String password, GenericConnectionFactory gcf )
      throws JMSException {
      super( userid, password, gcf );
   }

   // Constructor ---------------------------------------------------
   public SpyXAConnection( GenericConnectionFactory gcf )
      throws JMSException {
      super( gcf );
   }


   // Public --------------------------------------------------------

   public QueueSession createQueueSession( boolean transacted, int acknowledgeMode )
      throws JMSException {
      return ( QueueSession )createXAQueueSession();
   }


   /**
    *  createXAQueueSession method comment.
    *
    * @return                             Description of the Returned Value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public javax.jms.XAQueueSession createXAQueueSession()
      throws javax.jms.JMSException {

      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }
      checkClientID();
      
      XAQueueSession session = new SpyQueueSession( this, true, 0, true );

      //add the new session to the createdSessions list
      synchronized ( createdSessions ) {
         createdSessions.add( session );
      }

      return session;
   }

   //////////////////////////////////////////////////////////////////
   // Public Methods
   //////////////////////////////////////////////////////////////////

   public TopicSession createTopicSession( boolean transacted, int acknowledgeMode )
      throws JMSException {
      return ( TopicSession )createXATopicSession();
   }

   public XATopicSession createXATopicSession()
      throws javax.jms.JMSException {
      if ( closed ) {
         throw new IllegalStateException( "The connection is closed" );
      }
      checkClientID();
      
      XATopicSession session = new SpyTopicSession( this, true, 0, true );
      //add the new session to the createdSessions list
      synchronized ( createdSessions ) {
         createdSessions.add( session );
      }
      return session;
   }
}
