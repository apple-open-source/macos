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
import javax.jms.TopicConnection;
import javax.jms.TopicConnectionFactory;
import javax.jms.XAQueueConnection;
import javax.jms.XAQueueConnectionFactory;
import javax.jms.XATopicConnection;
import javax.jms.XATopicConnectionFactory;

import javax.naming.Reference;
import javax.naming.Referenceable;
import javax.naming.StringRefAddr;

import org.jboss.logging.Logger;

/**
 * This class implements <code>javax.jms.XATopicConnectionFactory</code>
 * and <code>javax.jms.XAQueueConnectionFactory</code>.
 *
 * @version    <tt>$Revision: 1.5 $</tt>
 * @created    August 16, 2001
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @author     <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class SpyXAConnectionFactory
   extends SpyConnectionFactory
   implements Serializable, XAQueueConnectionFactory, XATopicConnectionFactory
{
   private static final Logger log = Logger.getLogger(SpyXAConnectionFactory.class);
   
   // Constructor ---------------------------------------------------

   public SpyXAConnectionFactory( GenericConnectionFactory factory ) {
      super(factory);
   }
   
   public SpyXAConnectionFactory( Properties config ) {
      super(config);
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
            "org.jboss.mq.SpyXAConnectionFactory",
            new org.jboss.mq.referenceable.ObjectRefAddr( "DCF", factory ),
            "org.jboss.mq.referenceable.SpyConnectionFactoryObjectFactory", null );
   }


   /////////////////////////////////////////////////////////////////////////
   //                       XATopicConnectionFactory                      //
   /////////////////////////////////////////////////////////////////////////
   
   public XATopicConnection createXATopicConnection()
      throws JMSException
   {
      try {
         return new SpyXAConnection( factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException("Failed to create XATopicConnection", e);
      }
   }

   public XATopicConnection createXATopicConnection( String userName, String password )
      throws JMSException
   {
      try {
         if (userName == null)
            throw new SpyJMSException("Username is null");
         if (password == null)
            throw new SpyJMSException("Password is null");
         
         return new SpyXAConnection( userName, password, factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException("Failed to create XATopicConnection", e);
      }
   }

   
   /////////////////////////////////////////////////////////////////////////
   //                       XAQueueConnectionFactory                      //
   /////////////////////////////////////////////////////////////////////////
   
   public XAQueueConnection createXAQueueConnection()
      throws JMSException
   {
      try {
         return new SpyXAConnection( factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException("Failed to create XAQueueConnection", e);
      }
   }

   public XAQueueConnection createXAQueueConnection( String userName, String password )
      throws JMSException
   {
      try {
         if (userName == null)
            throw new SpyJMSException("Username is null");
         if (password == null)
            throw new SpyJMSException("Password is null");
         
         return new SpyXAConnection( userName, password, factory );
      }
      catch ( JMSException e ) {
         throw e;
      }
      catch ( Exception e ) {
         throw new SpyJMSException("Failed to create XAQueueConnection", e);
      }
   }
}
