/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.jdbc;

import java.net.URL;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Properties;
import java.util.TreeSet;
import javax.jms.JMSException;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.sql.DataSource;
import org.jboss.mq.ConnectionToken;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.pm.TxManager;
import org.jboss.mq.server.JMSDestination;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.server.MessageCache;
import org.jboss.mq.server.MessageReference;
import org.jboss.mq.xml.XElement;
import org.jboss.system.ServiceMBeanSupport;
import java.util.Map;

/**
 * This class manages all persistence related services for JDBC based
 * persistence.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean, org.jboss.mq.pm.PersistenceManagerMBean"
 *
 * @author Jayesh Parayali (jayeshpk1@yahoo.com)
 * @version $Revision: 1.19.2.1 $
 */
public class PersistenceManager 
   extends ServiceMBeanSupport 
   implements PersistenceManagerMBean, org.jboss.mq.pm.PersistenceManager 
{
   private ObjectName messageCacheName;
   private MessageCache messageCache;

   private ObjectName dataSourceName;
   private DataSource datasource;
   private String transactionTableName = "jms_transaction";
   private String messageTableName = "jms_messages";

   private String jmsDBPoolName;

   //we only need one- it has no state dependent on destination.
   private MessageLog messageLog;

   private Map unrestoredMessages;

   private TxManager txManager;


   // Object to handle transaction recording.
   TxLog txLog;
   // Maps SpyDestinations to SpyMessageLogs
   //HashMap messageLogs= new HashMap();
   // Maps (Long)txIds to LinkedList of AddFile tasks
   HashMap transactedTasks= new HashMap();


   public PersistenceManager() throws javax.jms.JMSException
   {
      txManager = new TxManager(this);
   }

   public Object getInstance()
   {
      return this;
   }

   public ObjectName getMessageCache()
   {
      return messageCacheName;
   }

   public void setMessageCache(ObjectName messageCache)
   {
      this.messageCacheName = messageCache;
   }

   public MessageCache getMessageCacheInstance()
   {
      return messageCache;
   }


   /**
    * @jmx:managed-attribute
    */
   public ObjectName getDataSource()
   {
      return dataSourceName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setDataSource(ObjectName dataSourceName)
   {
      this.dataSourceName = dataSourceName;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getMessageTableName()
   {
      return messageTableName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setMessageTableName(String tableName)
   {
      messageTableName = tableName;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getTransactionTableName()
   {
      return transactionTableName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setTransactionTableName(String tableName)
   {
      transactionTableName = tableName;
   }

   public void startService() throws Exception
   {
      //Find the ConnectionFactoryLoader MBean so we can find the datasource
      String dsName = (String)getServer().getAttribute(dataSourceName, "JndiName");
      //Get an InitialContext

      InitialContext ctx= new InitialContext();
      datasource= (DataSource) ctx.lookup("java:/" + dsName);
      txLog= new TxLog(datasource, transactionTableName);

      messageCache = (MessageCache)getServer().getAttribute(messageCacheName, "Instance");
      messageLog = new MessageLog(messageCache, datasource, messageTableName);
      restoreTransactions();

   }

   private void restoreTransactions() throws JMSException
   {
      Collection lostTx = txLog.restore();
      if (!lostTx.isEmpty())
      {
         log.error("Unrecoverable transactions found in jdbc persistence manager! Your data is corrupt!");
      } // end of if ()

      unrestoredMessages = messageLog.restoreAll();
   }

   public void restoreQueue(JMSDestination jmsDest, SpyDestination dest)
      throws javax.jms.JMSException
   {
      if (jmsDest == null)
      {
         throw new IllegalArgumentException("Must supply non null JMSDestination to restoreQueue");
      } // end of if ()
      if (dest == null)
      {
         throw new IllegalArgumentException("Must supply non null SpyDestination to restoreQueue");
      } // end of if ()
      Map messages = (Map)unrestoredMessages.get(dest.getName());
      if (messages != null)
      {
         for (Iterator i = messages.values().iterator(); i.hasNext();)
         {
            jmsDest.restoreMessage((MessageReference)i.next());
         } // end of for ()

      } // end of if ()

   }

   public void add(MessageReference messageRef, org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException {
      // LogInfo logInfo;
      SpyMessage message = messageRef.getMessage();
      /*
	  synchronized (messageLogs) {
		 logInfo= (LogInfo) messageLogs.get("" + message.getJMSDestination());
	  }

	  if (logInfo == null)
		 throw new javax.jms.JMSException("Destination was not initalized with the PersistenceManager");

	  logInfo.log.add(message, txId);
      */
      //messageLog will figure out what destination to use.
      messageLog.add(message,txId);

	  if (txId != null) {
		 LinkedList tasks;
		 synchronized (transactedTasks) {
			tasks= (LinkedList) transactedTasks.get(txId);
		 }
		 if (tasks == null)
			throw new javax.jms.JMSException("Transaction is not active 5.");
		 synchronized (tasks) {
			tasks.addLast(new Transaction(true, message, txId));
		 }
	  }

   }

   public void commitPersistentTx(org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException {

      LinkedList transacted;
      synchronized (transactedTasks) {
         transacted= (LinkedList) transactedTasks.remove(txId);
      }
      synchronized (transacted) {
         Iterator iter= transacted.iterator();
         while (iter.hasNext()) {
            Transaction task= (Transaction) iter.next();
            task.commit();
         }
      }

      txLog.commitTx(txId);
   }

   public org.jboss.mq.pm.Tx createPersistentTx() throws javax.jms.JMSException {
      org.jboss.mq.pm.Tx txId= txLog.createTx();
      synchronized (transactedTasks) {
         transactedTasks.put(txId, new LinkedList());
      }
      return txId;
   }

   /**
    * getTxManager method comment.
    */
   public org.jboss.mq.pm.TxManager getTxManager() {
      return txManager;
   }


   public void remove(MessageReference messageRef, org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException {
      // LogInfo logInfo;
      SpyMessage message = messageRef.getMessage();
      /*
	  synchronized (messageLogs) {
		 logInfo= (LogInfo) messageLogs.get("" + message.getJMSDestination());
	  }

	  if (logInfo == null)
		 throw new javax.jms.JMSException("Destination was not initalized with the PersistenceManager");
      */
	  if (txId == null)
      {
          //logInfo.log.remove(message, txId);

          // The message is removed from the database if there is no
          // transaction in the calling context. If there is a
          // transaction, the removal task is put into the task list
          // and waits to be executed until commit() is called on the
          // JMS Session. (@see Transaction)
          messageLog.remove(message, txId);
      }
	  else
      {
          LinkedList tasks;
          synchronized (transactedTasks) {
              tasks= (LinkedList) transactedTasks.get(txId);
          }
          if (tasks == null)
              throw new javax.jms.JMSException("Transaction is not active 6.");
          synchronized (tasks) {
              tasks.addLast(new Transaction(false, message, txId));
          }
	  }
   }

   public void update(MessageReference messageRef, org.jboss.mq.pm.Tx txId)
      throws javax.jms.JMSException
   {
      SpyMessage message = messageRef.getMessage();
      if (txId == null)
          messageLog.update(message, txId);
      else
          throw new JMSException("NYI: No code does updates in a transaction");
   }

   //not sure this one is used.
/*
   public void restore(org.jboss.mq.server.JMSServer server) throws javax.jms.JMSException {

	  TreeSet committingTXs= txLog.restore();
	  HashMap clone;
	  synchronized (messageLogs) {
		 clone= (HashMap) messageLogs.clone();
	  }

	  Iterator iter= clone.values().iterator();
	  while (iter.hasNext()) {

		 LogInfo logInfo= (LogInfo) iter.next();

		 JMSDestination q= server.getJMSDestination(logInfo.destination);

		 SpyMessage rebuild[]= logInfo.log.restore(committingTXs, q.toString());

		 org.jboss.mq.server.MessageCache cache = JMSServer.getInstance().getMessageCache();
		 //TODO: make sure this lock is good enough
		 synchronized (q) {
			for (int i= 0; i < rebuild.length; i++) {
			   MessageReference mr = cache.add(rebuild[i]);
			   q.restoreMessage(mr);
			}
		 }
	  }

   }
*/
   public void rollbackPersistentTx(org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException {

      LinkedList transacted;
      synchronized (transactedTasks) {
         transacted= (LinkedList) transactedTasks.remove(txId);
      }
      synchronized (transacted) {
         Iterator iter= transacted.iterator();
         while (iter.hasNext()) {
            Transaction task= (Transaction) iter.next();
            task.rollback();
         }
      }

      txLog.rollbackTx(txId);
   }


   /*static class LogInfo {
      MessageLog log;
      SpyDestination destination;

      LogInfo(MessageLog log, SpyDestination destination) {
         this.log= log;
         this.destination= destination;
      }
   }
   */
   class Transaction {
      //private LogInfo logInfo;
      private SpyMessage message;
      private org.jboss.mq.pm.Tx txId;
      private boolean add;
      public Transaction(boolean add, SpyMessage message, org.jboss.mq.pm.Tx txId) {
         this.add= add;
         //this.logInfo= logInfo;
         this.message= message;
         this.txId= txId;
      }
      public void commit() throws JMSException
      {
         // When the transaction is committed, we perform the actual
         // removal.
         if (!add)
         {
            messageLog.remove(message, txId);
         }
      }
      public void rollback() throws JMSException
      {
         // When the transaction is rollback, we need to remove the
         // added message from the persistence store.
         if (add)
         {
            messageLog.remove(message, txId);
         }
      }
   }

   /*
    * @see PersistenceManager#closeQueue(JMSDestination, SpyDestination)
    */
   public void closeQueue(JMSDestination jmsDest, SpyDestination dest) throws JMSException
   {
      // TODO: Do we need to cleanup anything when a queue get's closed?
   }

}
