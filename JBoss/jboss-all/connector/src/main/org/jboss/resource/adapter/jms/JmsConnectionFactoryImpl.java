/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import java.io.Serializable;

import javax.naming.Reference;

import javax.resource.Referenceable;
import javax.resource.ResourceException;
import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.ConnectionManager;

import javax.jms.JMSException;
import javax.jms.QueueConnection;
import javax.jms.TopicConnection;

import org.jboss.logging.Logger;

/**
 * The the connection factory implementation for the JMS RA.
 *
 * <p>
 * This object will be the QueueConnectionFactory or TopicConnectionFactory
 * which clients will use to create connections.
 *
 * Created: Thu Apr 26 17:02:50 2001
 * 
 * @version <tt>$Revision: 1.3 $</tt>
 * @author  <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class JmsConnectionFactoryImpl 
   implements JmsConnectionFactory, Referenceable
{
   private static final Logger log = Logger.getLogger(JmsConnectionFactoryImpl.class);

   private ManagedConnectionFactory mcf;

   private ConnectionManager cm;

   private Reference reference;

   public JmsConnectionFactoryImpl(final ManagedConnectionFactory mcf,
                                   final ConnectionManager cm) 
   {
      this.mcf = mcf;

      if (cm == null) {
         // This is standalone usage, no appserver
         this.cm = new JmsConnectionManager();
         if (log.isTraceEnabled()) {
            log.trace("Created new connection manager");
         }
      }
      else {
         this.cm = cm;
      }

      if (log.isTraceEnabled()) {
         log.trace("Using ManagedConnectionFactory=" + mcf + ", ConnectionManager=" + cm);
      }
   }

   public void setReference(final Reference reference) 
   {
      this.reference = reference;

      if (log.isTraceEnabled()) {
         log.trace("Using Reference=" + reference);
      }
   }
    
   public Reference getReference() 
   {
      return reference;
   }
   
   // --- QueueConnectionFactory
   
   public QueueConnection createQueueConnection() throws JMSException 
   {
      QueueConnection qc = new JmsSessionFactoryImpl(mcf, cm, false);

      if (log.isTraceEnabled()) {
         log.trace("Created queue connection: " + qc);
      }
      
      return qc;
   }
   
   public QueueConnection createQueueConnection(String userName, String password) 
      throws JMSException 
   {
      JmsSessionFactoryImpl s = new JmsSessionFactoryImpl(mcf, cm, false);
      s.setUserName(userName);
      s.setPassword(password);

      if (log.isTraceEnabled()) {
         log.trace("Created queue connection: " + s);
      }
      
      return s;
   } 

   // --- TopicConnectionFactory
   
   public TopicConnection createTopicConnection() throws JMSException 
   {
      TopicConnection tc = new JmsSessionFactoryImpl(mcf, cm, true);

      if (log.isTraceEnabled()) {
         log.trace("Created topci connection: " + tc);
      }

      return tc;
   }
   
   public TopicConnection createTopicConnection(String userName, String password)
      throws JMSException 
   {
      JmsSessionFactoryImpl s = new JmsSessionFactoryImpl(mcf, cm, true);
      s.setUserName(userName);
      s.setPassword(password);
      
      if (log.isTraceEnabled()) {
         log.trace("Created topic connection: " + s);
      }

      return s;
   }
}
