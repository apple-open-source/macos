/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import javax.naming.Reference;

import javax.resource.Referenceable;
import javax.resource.ResourceException;
import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ConnectionRequestInfo;

import javax.jms.JMSException;
import javax.jms.ConnectionConsumer;
import javax.jms.ServerSessionPool;
import javax.jms.TopicSession;
import javax.jms.Topic;
import javax.jms.QueueSession;
import javax.jms.Queue;
import javax.jms.ExceptionListener;
import javax.jms.ConnectionMetaData;

import org.jboss.logging.Logger;

/**
 * Implements the JMS Connection API and produces {@link JmsSession} objects.
 *
 * <p>Created: Thu Mar 29 15:36:51 2001
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author  <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class JmsSessionFactoryImpl
   implements JmsSessionFactory, Referenceable
{
   private static final Logger log = Logger.getLogger(JmsSessionFactoryImpl.class);
   
   private static final String ISE =
      "This method is not applicatable in JMS resource adapter";

   private Reference reference;

   // Used from JmsConnectionFactory
   private String userName;
   private String password;
   private boolean isTopic;
    
   /** JmsRa own factory */
   private ManagedConnectionFactory mcf;

   /** Hook to the appserver */
   private ConnectionManager cm;
    
   public JmsSessionFactoryImpl(final ManagedConnectionFactory mcf,
                                final ConnectionManager cm,
                                final boolean isTopic)
   {
      this.mcf = mcf;
      this.cm = cm;
      
      if (cm == null) {
         // This is standalone usage, no appserver
         this.cm = new JmsConnectionManager();
      }
      else {
         this.cm = cm;
      }

      this.isTopic = isTopic;

      log.debug("mcf=" + mcf + ", cm=" + cm + ", isTopic=" + isTopic);
   }

   public void setReference(final Reference reference) {
      this.reference = reference;
   }
    
   public Reference getReference() {
      return reference;
   }
    
   // --- API for JmsConnectionFactoryImpl
   
   public void setUserName(final String name) 
   {
      userName = name;
   }
    
   public void setPassword(final String password) 
   {
      this.password = password;
   }

   //---- QueueConnection ---
   
   public QueueSession createQueueSession(final boolean transacted, 
                                          final int acknowledgeMode) 
      throws JMSException
   { 
      try {
         if (isTopic) {
            throw new IllegalStateException
               ("Can not get a queue session from a topic connection");
         }
	    
         JmsConnectionRequestInfo info = 
            new JmsConnectionRequestInfo(transacted, acknowledgeMode, false);
         info.setUserName(userName);
         info.setPassword(password);
	    
         return (QueueSession)cm.allocateConnection(mcf, info);
      }
      catch (ResourceException e) {
         log.error("could not create session", e);

         JMSException je =
            new JMSException("Could not create a session: " + e);
         je.setLinkedException(e);
         throw je;
      }
   }
    
   public ConnectionConsumer createConnectionConsumer
      (Queue queue,
       String messageSelector,
       ServerSessionPool sessionPool,
       int maxMessages) 
      throws JMSException 
   {
      throw new IllegalStateException(ISE);
   }
    
   //--- TopicConnection ---
    
   public TopicSession createTopicSession(final boolean transacted, 
                                          final int acknowledgeMode) 
      throws JMSException
   { 
      try {
         if (!isTopic) {
            throw new IllegalStateException
               ("Can not get a topic session from a session connection");
         }

         JmsConnectionRequestInfo info = 
            new JmsConnectionRequestInfo(transacted, acknowledgeMode, true);
         
         info.setUserName(userName);
         info.setPassword(password);
	    
         return (TopicSession)cm.allocateConnection(mcf, info);
      }
      catch (ResourceException e) {
         log.error("could not create session", e);
         
         JMSException je = new JMSException
            ("Could not create a session: " + e);
         je.setLinkedException(e);
         throw je;
      }				    
   }

   public ConnectionConsumer createConnectionConsumer
      (Topic topic,
       String messageSelector,
       ServerSessionPool sessionPool,
       int maxMessages) 
      throws JMSException 
   {
      throw new IllegalStateException(ISE);
   }		       

   public ConnectionConsumer createDurableConnectionConsumer
      (Topic topic, 
       String subscriptionName,
       String messageSelector,
       ServerSessionPool sessionPool, 
       int maxMessages) 
      throws JMSException
   {
      throw new IllegalStateException(ISE);
   }
   
   //--- All the Connection methods
   
   public String getClientID() throws JMSException {
      throw new IllegalStateException(ISE);
   }
    
   public void setClientID(String cID) throws JMSException {
      throw new IllegalStateException(ISE);
   }
    
   public ConnectionMetaData getMetaData() throws JMSException {
      throw new IllegalStateException(ISE);
   }
    
   public ExceptionListener getExceptionListener() throws JMSException {
      throw new IllegalStateException(ISE);
   }
    
   public void setExceptionListener(ExceptionListener listener)
      throws JMSException
   {
      throw new IllegalStateException(ISE);
   }
    
   public void start() throws JMSException {
      throw new IllegalStateException(ISE);
   }
    
   public void stop() throws JMSException {
      throw new IllegalStateException(ISE);
   }

   public void close() throws JMSException {
      //
      // TODO: close all sessions, for now just do nothing.
      //
   }
}
