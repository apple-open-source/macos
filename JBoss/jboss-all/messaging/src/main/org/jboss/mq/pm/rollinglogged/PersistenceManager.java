/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.pm.rollinglogged;

import java.io.File;
import java.net.URL;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.TreeSet;
import javax.jms.JMSException;
import javax.management.ObjectName;
import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.pm.TxManager;
import org.jboss.mq.server.JMSDestination;
import org.jboss.mq.server.JMSQueue;
import org.jboss.mq.server.JMSTopic;
import org.jboss.mq.server.PersistentQueue;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.mq.server.MessageReference;
import org.jboss.mq.server.MessageCache;

import org.jboss.system.server.ServerConfigLocator;

/**
 * This class manages all persistence related services.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean, org.jboss.mq.pm.PersistenceManagerMBean"
 *
 * @author David Maplesden (David.Maplesden@orion.co.nz)
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.25.2.4 $
 */
public class PersistenceManager
   extends ServiceMBeanSupport
   implements org.jboss.mq.pm.PersistenceManager, PersistenceManagerMBean
{
   public final static String TRANS_FILE_NAME = "transactions.dat";

   protected static int MAX_POOL_SIZE = 50;

   private ObjectName messageCacheName;
   private MessageCache messageCache;

   protected java.util.ArrayList listPool = new java.util.ArrayList();
   protected java.util.ArrayList txPool = new java.util.ArrayList();

   protected int messageCounter = 0;
   int numRollOvers = 0;
   HashMap queues = new HashMap();
   // Log file used to store committed transactions.
   SpyTxLog currentTxLog;
   long nextTxId = Long.MIN_VALUE;
   // Maps txLogs to Maps of SpyDestinations to SpyMessageLogs
   HashMap messageLogs = new HashMap();

   // Maps transactionIds to txInfos
   HashMap transToTxLogs = new HashMap();

   // The directory where persistence data should be stored
   File dataDir;
   TxManager txManager;

   private String dataDirectory;
   private int rollOverSize;

   private HashMap unrestoredMessages = new HashMap();


   /**
    *  NewPersistenceManager constructor.
    *
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public PersistenceManager()
      throws javax.jms.JMSException
   {
      txManager = new TxManager(this);
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
    * Sets the DataDirectory attribute of the PersistenceManagerMBean object
    *
    * @jmx:managed-attribute
    *
    * @param  newDataDirectory  The new DataDirectory value
    */
   public void setDataDirectory(String newDataDirectory)
   {
      dataDirectory = newDataDirectory;
   }

   /**
    * Gets the DataDirectory attribute of the PersistenceManagerMBean object
    *
    * @jmx:managed-attribute
    *
    * @return    The DataDirectory value
    */
   public String getDataDirectory()
   {
      return dataDirectory;
   }

   /**
    * Sets the maximum number of messages before log rolls over
    *
    * @jmx:managed-attribute
    *
    * @param   rollOverSize   The maximum number of messages before
    *                         rollover occurs
    */
   public void setRollOverSize( int rollOverSize )
   {
      this.rollOverSize = rollOverSize;
   }

   /**
    * Gets maximum number of messages until log rolls over
    *
    * @jmx:managed-attribute
    *
    * @return     number of messages before log rolls over
    */
   public int getRollOverSize()
   {
      return rollOverSize;
   }

   /**
    * Returns this instance.
    *
    * @jmx:managed-attribute
    *
    * @return     this
    */
   public Object getInstance()
   {
      return this;
   }

   /**
    *  getTxManager method comment.
    *
    * @return    The TxManager value
    */
   public org.jboss.mq.pm.TxManager getTxManager()
   {
      return txManager;
   }

   /**
    * Setup the data directory, where messages will be stored, connects
    * to the message cache and restores transactions.
    */
   public void startService()
      throws Exception
   {
      log.debug("Using new rolling logged persistence manager.");
      
      dataDir = null;
      // First check if the given Data Directory is a valid URL pointing to
      // a read and writable directory
      try
      {
         URL fileURL = new URL(dataDirectory);
         File file = new File(fileURL.getFile());
         if (file.isDirectory() && file.canRead() && file.canWrite())
         {
            dataDir = file;
            if (log.isDebugEnabled())
            {
               log.debug("Using data directory: " + dataDir);
            }
         }
      }
      catch (Exception e)
      {
         // Ignore message and try it as relative path
      }

      // Create the data dir under the server home if it was not a valid URL
      if (dataDir == null)
      {
         // Get the system home directory
         File systemHomeDir = ServerConfigLocator.locate().getServerHomeDir();

         dataDir = new File(systemHomeDir, dataDirectory);
         if (log.isDebugEnabled())
         {
            log.debug("Using data directory: " + dataDir);
         }

         dataDir.mkdirs();
         if (!dataDir.isDirectory())
            throw new Exception("The data directory is not valid: " + dataDir.getCanonicalPath());
      }
      
      messageCache = (MessageCache) getServer().getAttribute( messageCacheName, "Instance" );
      
      restoreTransactions();
   }

   /**
    *  #Description of the Method
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void add(MessageReference messageRef, org.jboss.mq.pm.Tx txId)
      throws javax.jms.JMSException
   {
      SpyMessage message = messageRef.getMessage();
      LogInfo logInfo;

      SpyTxLog txLog = null;
      if (txId == null)
      {
         txLog = currentTxLog;
      }
      else
      {
         synchronized (transToTxLogs)
         {
            txLog = ((TxInfo)transToTxLogs.get(txId)).log;
         }
      }

      HashMap logs;
      synchronized (messageLogs)
      {
         logs = (HashMap)messageLogs.get(txLog);
      }
      synchronized (logs)
      {
         logInfo = (LogInfo)logs.get(message.getJMSDestination().toString());
      }

      if (logInfo == null)
      {
         throw new javax.jms.JMSException("Destination was not initalized with the PersistenceManager");
      }

      synchronized (logInfo)
      {
         logInfo.liveMessages++;
         messageRef.persistData = logInfo;
         logInfo.log.add(message, txId);
      }
      if (txId != null)
      {
         synchronized (transToTxLogs)
         {
            TxInfo txInfo = (TxInfo)transToTxLogs.get(txId);
            txInfo.addMessages.add(message);
         }
      }
      checkRollOver();
   }

   /**
    *  #Description of the Method
    *
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void commitPersistentTx(org.jboss.mq.pm.Tx txId)
      throws javax.jms.JMSException
   {
      TxInfo info = null;
      LinkedList messagesToDelete = null;
      synchronized (transToTxLogs)
      {
         info = (TxInfo)transToTxLogs.remove(txId);
         messagesToDelete = info.ackMessages;
      }
      deleteMessages(messagesToDelete);
      info.log.commitTx(txId);
      synchronized (transToTxLogs)
      {
         releaseTx(txId);
         releaseTxInfo(info);
      }
      checkCleanup();//info.log);
   }

   /**
    *  #Description of the Method
    *
    * @return                             Description of the Returned Value
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public org.jboss.mq.pm.Tx createPersistentTx()
      throws javax.jms.JMSException
   {
      org.jboss.mq.pm.Tx txId = null;
      SpyTxLog txLog = currentTxLog;
      synchronized (transToTxLogs)
      {
         txId = getTx(++nextTxId);
         transToTxLogs.put(txId, getTxInfo(txId, txLog));
      }
      txLog.createTx();
      return txId;
   }

   /**
    *  #Description of the Method
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void remove(MessageReference messageRef, org.jboss.mq.pm.Tx txId)
      throws javax.jms.JMSException
   {
      SpyMessage message = messageRef.getMessage();
      LogInfo logInfo;

      SpyTxLog txLog = ((LogInfo)messageRef.persistData).txLog;
      synchronized (messageLogs)
      {
         HashMap logs = (HashMap)messageLogs.get(txLog);
         if (logs == null) 
         {
            log.error("keys for messageLogs are:");
            for (Iterator i = messageLogs.keySet().iterator(); i.hasNext();) 
            {
               log.error(i.next().toString());
            } // end of for ()
            
            throw new JMSException("no logs for this txLog: " + txLog);
         } // end of if ()
         String destName = message.getJMSDestination().toString();
         logInfo = (LogInfo)logs.get(destName);
      }

      if (logInfo == null)
      {
         throw new javax.jms.JMSException("Destination was not initalized with the PersistenceManager");
      }

      synchronized (logInfo.log)
      {
         logInfo.log.remove(message, txId);
      }
      if (txId != null)
      {
         synchronized (transToTxLogs)
         {
            TxInfo txInfo = (TxInfo)transToTxLogs.get(txId);
            txInfo.ackMessages.add(messageRef);
         }
      }
      if (txId == null)
      {
         synchronized (logInfo)
         {
            --logInfo.liveMessages;
         }
         //checkCleanup(txLog); maybe only do this on rollover
      }
   }

   /**
    *  Update a message
    *
    * @param  message                     the reference message
    * @param  txId                        the transaction id
    * @exception  javax.jms.JMSException  for any error
    */
   public void update(MessageReference messageRef, org.jboss.mq.pm.Tx txId)
      throws javax.jms.JMSException
   {
      SpyMessage message = messageRef.getMessage();
      LogInfo logInfo;

      SpyTxLog txLog = ((LogInfo)messageRef.persistData).txLog;
      synchronized (messageLogs)
      {
         HashMap logs = (HashMap)messageLogs.get(txLog);
         if (logs == null) 
         {
            log.error("keys for messageLogs are:");
            for (Iterator i = messageLogs.keySet().iterator(); i.hasNext();) 
            {
               log.error(i.next().toString());
            } // end of for ()
            
            throw new JMSException("no logs for this txLog: " + txLog);
         } // end of if ()
         String destName = message.getJMSDestination().toString();
         logInfo = (LogInfo)logs.get(destName);
      }

      if (logInfo == null)
      {
         throw new javax.jms.JMSException("Destination was not initalized with the PersistenceManager");
      }

      synchronized (logInfo.log)
      {
         logInfo.log.update(message, txId);
      }
      if (txId != null)
      {
         throw new JMSException("NYI: No code does updates in a transaction");
      }
   }

   public void restoreTransactions()
      throws javax.jms.JMSException
   {
      TreeSet committedTxs = new TreeSet();
      HashMap txLogs = new HashMap();
      java.io.File dir = dataDir;
      java.io.File[] dataFiles = dir.listFiles();

      for (int i = 0; i < dataFiles.length; ++i)
      {
         String name = dataFiles[i].getName();
         if (name.startsWith(TRANS_FILE_NAME))
         {
            int index = name.lastIndexOf(".dat");
            if (index < 0)
            {
               continue;
            }
            String sRollOver = name.substring(index + 4);
            int rollOver = Integer.parseInt(sRollOver);
            numRollOvers = Math.max(numRollOvers, rollOver);
            SpyTxLog txLog = new SpyTxLog(dataFiles[i]);
            txLog.restore(committedTxs);
            txLogs.put(new Integer(rollOver), txLog);
            messageLogs.put(txLog, new HashMap());
         }
      }

      if (!committedTxs.isEmpty())
      {
         nextTxId = ((org.jboss.mq.pm.Tx)committedTxs.last()).longValue();
      }
      //now "pre-restore" message logs
      for (int i = 0; i < dataFiles.length; ++i)
      {
         //message log names look like <queuename>.dat<rollovercounter>
         //4 = length(".dat");
         String name = dataFiles[i].getName();
         int index = name.lastIndexOf(".dat");
         if (index < 0)
         {
            continue;
         }
         String sRollOver = name.substring(index + 4);
         int rollOver = Integer.parseInt(sRollOver);
         //key is retrieved queue name.
         String key = name.substring(0, name.length() - (sRollOver.length() + 4));
         if (!name.startsWith(TRANS_FILE_NAME))
         {
            HashMap messages = (HashMap)unrestoredMessages.get(key);
            if (messages == null) 
            {
               messages = new HashMap();
               unrestoredMessages.put(key, messages);
            } // end of if ()
            
            SpyMessageLog messageLog = new SpyMessageLog(messageCache, dataFiles[i]);
            SpyTxLog txLog = (SpyTxLog)txLogs.get(new Integer(rollOver));
            if (txLog == null) 
            {
               log.warn("no transaction log for message log " + dataFiles[i]);
               continue;
            } // end of if ()            
            LogInfo info = new LogInfo(messageLog, null, txLog);
            messageLog.restore(committedTxs, info, messages);
            HashMap logs = (HashMap)messageLogs.get(txLog);
            logs.put(key, info);
            unrestoredMessages.put(key, messages);
         }
      }
      //set up rolled over logs for new transactions.
      rollOverLogs();
   }

   public void restoreDestination(JMSDestination jmsDest)
      throws javax.jms.JMSException
   {
      if (jmsDest instanceof JMSQueue) 
      {
         SpyDestination spyDest = jmsDest.getSpyDestination();
         restoreQueue(jmsDest, spyDest);
      } // end of if ()
      else if (jmsDest instanceof JMSTopic) 
      {
         ArrayList persistQList = ((JMSTopic)jmsDest).getPersistentQueues();
         Iterator pq = persistQList.iterator();
         while (pq.hasNext()) 
         {
            SpyDestination spyDest = ((PersistentQueue)pq.next()).getSpyDestination();

            restoreQueue(jmsDest, spyDest);

         } // end of while ()
         
      } // end of if ()
      
      //now see if we have restored all the preexisting queues

      if (unrestoredMessages.isEmpty()) 
      {
         checkCleanup(); 
      } // end of if () 
   }

   public void restoreQueue(JMSDestination jmsDest, SpyDestination dest)
      throws JMSException
   {

      //remember this queue
      String queueName = dest.toString();
      queues.put(queueName, dest);
      //set the info.destination on all the logInfos for this queue
      Iterator txLogIt = messageLogs.keySet().iterator();
      while (txLogIt.hasNext()) 
      {
         SpyTxLog txLog = (SpyTxLog)txLogIt.next();
         HashMap logs = (HashMap)messageLogs.get(txLog);
         LogInfo info = (LogInfo)logs.get(queueName);
         if (info != null) 
         {
            info.destination = dest;
         } // end of if ()

      } // end of while ()
      //restore the messages from old logs (previously read into unrestoredMessages)
      HashMap messages = (HashMap)unrestoredMessages.remove(queueName);
      if (messages != null) 
      {
         
      
         synchronized (jmsDest)
         {
            Iterator m = messages.values().iterator();
            while (m.hasNext()) 
            {
               MessageReference message = (MessageReference)m.next();
               if (dest instanceof org.jboss.mq.SpyTopic)
               {
               	  SpyMessage sm = message.getMessage();
                  sm.header.durableSubscriberID = ((org.jboss.mq.SpyTopic)dest).getDurableSubscriptionID();
                  message.invalidate(); // since we did an update.
                  //message.durableSubscriberID = ((org.jboss.mq.SpyTopic)dest).getDurableSubscriptionID();
               }
               jmsDest.restoreMessage(message);
            } // end of while ()

         }
      } // end of if ()
      //set up new log file for restored (or new) queue
      synchronized (messageLogs)
      {
         HashMap logs = (HashMap)messageLogs.get(currentTxLog);
         logs.put(queueName, newQueueInfo(dest, currentTxLog));
      }
   }

   /**
    *  #Description of the Method
    *
    * @param  txId                        Description of Parameter
    * @exception  javax.jms.JMSException  Description of Exception
    */
   public void rollbackPersistentTx(org.jboss.mq.pm.Tx txId)
      throws javax.jms.JMSException
   {
      TxInfo info = null;
      LinkedList messagesToDelete = null;
      synchronized (transToTxLogs)
      {
         info = (TxInfo)transToTxLogs.remove(txId);
         messagesToDelete = info.addMessages;
      }
      deleteMessages(messagesToDelete);
      info.log.rollbackTx(txId);
      synchronized (transToTxLogs)
      {
         releaseTx(txId);
         releaseTxInfo(info);
      }
      //checkCleanup(info.log);maybe only on rollover
   }

   protected org.jboss.mq.pm.Tx getTx(long value)
   {
      if (txPool.isEmpty())
      {
         return new org.jboss.mq.pm.Tx(value);
      }
      else
      {
         org.jboss.mq.pm.Tx tx = (org.jboss.mq.pm.Tx)txPool.remove(txPool.size() - 1);
         tx.setValue(value);
         return tx;
      }
   }

   protected TxInfo getTxInfo(org.jboss.mq.pm.Tx txId, SpyTxLog txLog)
   {
      if (listPool.isEmpty())
      {
         return new TxInfo(txId, txLog);
      }
      else
      {
         TxInfo info = (TxInfo)listPool.remove(listPool.size() - 1);
         info.txId = txId;
         info.log = txLog;
         return info;
      }
   }

   protected void releaseTxInfo(TxInfo list)
   {
      if (listPool.size() < MAX_POOL_SIZE)
      {
         list.ackMessages.clear();
         list.addMessages.clear();
         listPool.add(list);
      }
   }

   protected void deleteMessages(LinkedList messages) throws javax.jms.JMSException
   {
      for (Iterator it = messages.iterator(); it.hasNext(); )
      {
         LogInfo info = ((LogInfo)((MessageReference)it.next()).persistData);
         synchronized (info)
         {
            --info.liveMessages;
         }
         //checkCleanup(info.txLog);maybe only on rollover
      }
   }

   protected void checkRollOver() throws JMSException
   {
      synchronized (queues)
      {
         int max = queues.size();
         if (max == 0)
         {
            max = rollOverSize;
         }
         else
         {
            max *= rollOverSize;
         }
         if (++messageCounter > max)
         {
            messageCounter = 0;
            rollOverLogs();
         }
      }
   }

   protected void rollOverLogs() throws JMSException
   {
      try
      {
         HashMap logs = new HashMap();
         ++numRollOvers;
         SpyTxLog newTxLog = new SpyTxLog(new File(dataDir, TRANS_FILE_NAME + numRollOvers));

         for (Iterator it = queues.values().iterator(); it.hasNext(); )
         {
            SpyDestination spyDest = (SpyDestination)it.next();
            logs.put(spyDest.toString(), newQueueInfo(spyDest, newTxLog));
         }
         SpyTxLog oldLog = currentTxLog;
         synchronized (messageLogs)
         {
            currentTxLog = newTxLog;
            messageLogs.put(newTxLog, logs);
         }
         checkCleanup();//oldLog);
      }
      catch (Exception e)
      {
         JMSException jme = new SpyJMSException("Error rolling over logs to new files.");
         jme.setLinkedException(e);
         throw jme;
      }
   }

   protected LogInfo newQueueInfo(SpyDestination spyDest, SpyTxLog txLog) throws JMSException
   {
      try 
      {
         String destName = spyDest.toString();
         SpyMessageLog log = new SpyMessageLog(messageCache, new File(dataDir, destName + ".dat" + numRollOvers));
         return new LogInfo(log, spyDest, txLog);
      } 
      catch (Exception e) 
      {
         JMSException jme = new SpyJMSException("Error rolling over log to new file for dest: " + spyDest);
         jme.setLinkedException(e);
         throw jme;
      } // end of try-catch
      
   }

   protected void checkCleanup() throws JMSException
   {

      Iterator logs = null;
      synchronized(messageLogs){
         logs = new ArrayList(messageLogs.keySet()).iterator();
      }
      while (logs.hasNext()) 
      {
         checkCleanup((SpyTxLog)logs.next());
      } // end of while ()
   }

   protected void checkCleanup(SpyTxLog txLog) throws JMSException
   {
      if (txLog == null || txLog == currentTxLog)
      {
         return;
      }
      Map logs;
      synchronized (messageLogs)
      {
         logs = (Map)messageLogs.get(txLog);
      }
      if (logs == null)
      {
          // Could another checkCleanup have removed this log?
          log.debug("Looks like this log was already removed");
          return;
      }
      synchronized (logs)
      {
         //if no live messages and no live transactions then cleanup
         for (Iterator it = logs.values().iterator(); it.hasNext(); )
         {
            LogInfo info = (LogInfo)it.next();
            synchronized (info)
            {
               if (info.liveMessages != 0)
               {
                  return;
               }
            }
         }
      }
      if (!txLog.completed())
      {
         return;
      }
      if ( log.isDebugEnabled() )
      {
         log.debug( "Cleaning up" );
      }
      //close and delete all logs, remove data from data structures.
      synchronized (messageLogs)
      {
         logs = (Map)messageLogs.remove(txLog);
      }
      if (logs == null)
      {
         return;
      }
      txLog.close();
      txLog.delete();
      for (Iterator it = logs.values().iterator(); it.hasNext(); )
      {
         LogInfo info = (LogInfo)it.next();
         info.log.close();
         info.log.delete();
      }
   }

   protected void releaseTx(org.jboss.mq.pm.Tx tx)
   {
      if (txPool.size() < MAX_POOL_SIZE)
      {
         txPool.add(tx);
      }
   }

   /**
    *  #Description of the Class
    */
   static class LogInfo
   {
      SpyMessageLog log;
      SpyDestination destination;
      int liveMessages = 0;
      SpyTxLog txLog;

      LogInfo(SpyMessageLog log, SpyDestination destination, SpyTxLog txLog)
      {
         this.log = log;
         this.destination = destination;
         this.txLog = txLog;
      }

   }

   /**
    *  #Description of the Class
    */
   static class TxInfo
   {
      org.jboss.mq.pm.Tx txId;
      LinkedList addMessages = new LinkedList();
      LinkedList ackMessages = new LinkedList();
      SpyTxLog log;

      TxInfo(org.jboss.mq.pm.Tx txId, SpyTxLog log)
      {
         this.txId = txId;
         this.log = log;
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
/*
vim:tabstop=3:et:shiftwidth=3
*/
