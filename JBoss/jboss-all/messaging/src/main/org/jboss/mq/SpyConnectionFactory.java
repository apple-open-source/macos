/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq;

import java.io.Serializable;
import java.util.Properties;

import javax.jms.JMSException;
import javax.jms.QueueConnection;
import javax.jms.QueueConnectionFactory;
import javax.jms.TopicConnection;
import javax.jms.TopicConnectionFactory;

import javax.naming.Reference;
import javax.naming.Referenceable;
import javax.naming.StringRefAddr;

import org.jboss.logging.Logger;

/**
 * This class implements <code>javax.jms.TopicConnectionFactory</code>
 * and <code>javax.jms.QueueConnectionFactory</code>.
 *
 * @version    <tt>$Revision: 1.5 $</tt>
 * @created    August 16, 2001
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class SpyConnectionFactory
   implements Serializable, QueueConnectionFactory, TopicConnectionFactory, Referenceable
{
   private static final Logger log = Logger.getLogger(SpyConnectionFactory.class);

   // Attributes ----------------------------------------------------

   protected GenericConnectionFactory factory;

   // Constructor ---------------------------------------------------

   public SpyConnectionFactory( GenericConnectionFactory factory ) {
      this.factory = factory;
   }
   
   public SpyConnectionFactory( Properties config ) {
      this.factory = new GenericConnectionFactory(null, config);
   }

   /**
    *  getReference method - to implement javax.naming.Refrenceable
    *
    * @return                                   The Reference value
    * @exception  javax.naming.NamingException  Description of Exception
    */
   public Reference getReference()
      throws javax.naming.NamingException
   {
      return new Reference(
            "org.jboss.mq.SpyConnectionFactory",
            new org.jboss.mq.referenceable.ObjectRefAddr( "DCF", factory ),
            "org.jboss.mq.referenceable.SpyConnectionFactoryObjectFactory", null );
   }


   /////////////////////////////////////////////////////////////////////////
   //                        TopicConnectionFactory                       //
   /////////////////////////////////////////////////////////////////////////

   public TopicConnection createTopicConnection()
      throws JMSException
   {
      try {
         return new SpyConnection( factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException( "Failed to create TopicConnection", e);
      }
   }

   public TopicConnection createTopicConnection( String userName, String password )
      throws JMSException
   {
      try {
         if (userName == null)
            throw new SpyJMSException("Username is null");
         if (password == null)
            throw new SpyJMSException("Password is null");
         
         return new SpyConnection( userName, password, factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException( "Failed to create TopicConnection", e );
      }
   }

   
   /////////////////////////////////////////////////////////////////////////
   //                        QueueConnectionFactory                       //
   /////////////////////////////////////////////////////////////////////////
   
   public QueueConnection createQueueConnection()
      throws JMSException
   {
      try {
         return new SpyConnection( factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException( "Failed to create QueueConnection", e );
      }
   }

   public QueueConnection createQueueConnection( String userName, String password )
      throws JMSException
   {
      try {
         if (userName == null)
            throw new SpyJMSException("Username is null");
         if (password == null)
            throw new SpyJMSException("Password is null");

         return new SpyConnection( userName, password, factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException( "Failed to create QueueConnection", e );
      }
   }
}
