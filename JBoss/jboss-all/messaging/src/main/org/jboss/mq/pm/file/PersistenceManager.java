/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.pm.file;

import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URL;
import java.text.NumberFormat;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.ArrayList;
import java.util.Map;
import java.util.TreeSet;
import javax.jms.JMSException;
import javax.management.ObjectName;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.pm.TxManager;
import org.jboss.mq.server.JMSDestination;
import org.jboss.mq.server.JMSQueue;
import org.jboss.mq.server.JMSDestinationManager;
import org.jboss.mq.server.JMSTopic;
import org.jboss.mq.server.PersistentQueue;
import org.jboss.mq.server.MessageReference;
import org.jboss.mq.server.MessageCache;

import org.jboss.system.ServiceMBeanSupport;

import org.jboss.system.server.ServerConfigLocator;

/**
 * This class manages all persistence related services for file based
 * persistence.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean, org.jboss.mq.pm.PersistenceManagerMBean"
 *
 * @author     Paul Kendall (paul.kendall@orion.co.nz)
 * @version    $Revision: 1.26.2.1 $
 */
public class PersistenceManager
   extends ServiceMBeanSupport
   implements PersistenceManagerMBean, org.jboss.mq.pm.PersistenceManager
{
   protected final static int MAX_POOL_SIZE = 50;

   private ObjectName messageCacheName;
   private MessageCache messageCache;

   protected ArrayList txPool = new ArrayList();

   protected long tidcounter = Long.MIN_VALUE;

   /**
    * The sub-directory under the system home where persistence
    * data will be stored.
    */
   String dataDirectory;

   /** A reference to the actual data directory. */
   File dataDir;
   
   /** tx manager. */
   org.jboss.mq.pm.TxManager txManager;
   
   /** Maps SpyDestinations to SpyMessageLogs */
   HashMap messageLogs = new HashMap();
   
   /** Maps (Long)txIds to LinkedList of AddFile tasks */
   HashMap transactedTasks = new HashMap();
   
   /** Holds unrestored messages read from queues, indexed by queue name. */
   Map unrestoredMessages = new HashMap();

   /**
    * Sets up the transaction manager.
    */
   public PersistenceManager() throws JMSException
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
    *  Sets the DataDirectory attribute of the PersistenceManager object
    *
    * @param  newDataDirectory  The new DataDirectory value
    *
    * @jmx:managed-attribute
    */
   public void setDataDirectory(String newDataDirectory)
   {
      dataDirectory = newDataDirectory;
   }

   /**
    *  Gets the DataDirectory attribute of the PersistenceManager object
    *
    * @return    The DataDirectory value
    *
    * @jmx:managed-attribute
    */
   public String getDataDirectory()
   {
      return dataDirectory;
   }

   /**
    *  Gets the TxManager attribute of the PersistenceManager object
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
   public void startService() throws Exception
   {
      dataDir = null;
      // First check if the given Data Directory is a valid URL pointing to
      // a read and writable directory
      try {
         URL fileURL = new URL( dataDirectory );
         File file = new File( fileURL.getFile() );
         if( file.isDirectory() && file.canRead() && file.canWrite() ) {
            dataDir = file;
            if (log.isDebugEnabled()) {
               log.debug("Using data directory: " + dataDir);
            }
         }
      }
      catch( Exception e ) {
         // Ignore message and try it as relative path
      }
      if( dataDir == null ) {
         // Get the system home directory
         File systemHomeDir = ServerConfigLocator.locate().getServerHomeDir();
   
         dataDir = new File(systemHomeDir, dataDirectory);
         if (log.isDebugEnabled()) {
            log.debug("Using data directory: " + dataDir);
         }
   
         dataDir.mkdirs();
         if (!dataDir.isDirectory())
            throw new Exception("The data directory is not valid: " + dataDir.getCanonicalPath());
      }

      messageCache = (MessageCache)
         getServer().getAttribute(messageCacheName, "Instance");

      restoreTransactions();
   }

   /**
    * The <code>restoreTransactions</code> method is called when the 
    * PersistenceManager service is started.  It reads all transaction log
    * files, and pre-restores all messages that are committed and not read.
    * When a queue or topic is started, it will collect these pre-restored
    * messages and add them to its in memory queue.
    *
    * @exception JMSException if an error occurs
    */
   private void restoreTransactions()  throws JMSException
   {
      boolean debug = log.isDebugEnabled();

      TreeSet txs = new TreeSet();
      File[] transactFiles = dataDir.listFiles();
      int queueNameOffset = dataDir.toString().length()+1;
      if(transactFiles != null)
      {
         for (int i = 0; i < transactFiles.length; i++)
         {
            // Set up a messageLog for each queue data directory.
            if( transactFiles[i].isDirectory() )
            {
               String dirName = transactFiles[i].toString();
               String key = dirName.substring(queueNameOffset);
               MessageLog msgLog = new MessageLog(messageCache, transactFiles[i]);
               LogInfo info = new LogInfo(msgLog, null);
               synchronized (messageLogs)
               {
                  messageLogs.put(key, info);
               }
               transactFiles[i] = null;
               continue;
            }

            try
            {
               Long tx = new Long(Long.parseLong(transactFiles[i].getName()));
               ArrayList removingMessages = readTxFile(transactFiles[i]);
               if (testRollBackTx(tx, removingMessages))
               {
                  txs.add(tx);
               }
            }
            catch (NumberFormatException e)
            {
               log.warn("Ignoring invalid transaction record file " + transactFiles[i].getAbsolutePath());
               transactFiles[i] = null;
            }
            catch (IOException e)
            {
               JMSException jmse = new SpyJMSException("IO Error when restoring.");
               jmse.setLinkedException(e);
               throw jmse;
            }
         }
      }
      if (!txs.isEmpty())
      {
         this.tidcounter = ((Long)txs.last()).longValue() + 1;
      }

      HashMap clone;
      synchronized (messageLogs)
      {
         clone = (HashMap)messageLogs.clone();
      }

      for (Iterator i = clone.keySet().iterator(); i.hasNext();)
      {
         Object key = i.next();
         LogInfo logInfo = (LogInfo)clone.get(key);
         if (debug)
            log.debug("Recovered messages destined for: "+key);
         unrestoredMessages.put(key, logInfo.log.restore(txs));
      }

      //all txs now committed or rolled back so delete tx files
      if(transactFiles != null)
      {
         for (int i = 0; i < transactFiles.length; i++)
         {
            if (transactFiles[i] != null)
            {
               deleteTxFile(transactFiles[i]);
            }
         }
      }

   }

   /**
    * The <code>restoreDestination</code> method is called by a queue or 
    * topic on startup.  The method sends all the pre-restored messages to
    * the JMSDestination to get them back into the in-memory queue.
    *
    * @param jmsDest a <code>JMSDestination</code> value
    * @exception JMSException if an error occurs
    */
   public void restoreDestination(JMSDestination jmsDest) throws JMSException
   {
      if (jmsDest instanceof JMSQueue) 
      {
         SpyDestination spyDest = jmsDest.getSpyDestination();
         restoreQueue(jmsDest, spyDest);
      }
      else if (jmsDest instanceof JMSTopic) 
      {
         Collection persistQList = ((JMSTopic)jmsDest).getPersistentQueues();
         Iterator pq = persistQList.iterator();
         while (pq.hasNext()) 
         {
            SpyDestination spyDest = ((PersistentQueue)pq.next()).getSpyDestination();

            restoreQueue(jmsDest, spyDest);
         }
      }
   }

   /**
    * The <code>restoreQueue</code> method restores the messages for
    * a SpyDestination to its queue by sending them to the associated
    * JMSDestination.
    *
    * @param jmsDest a <code>JMSDestination</code> value
    * @param dest a <code>SpyDestination</code> value
    * @exception JMSException if an error occurs
    */
   public void restoreQueue(JMSDestination jmsDest, SpyDestination dest)
      throws JMSException
   {
      boolean debug = log.isDebugEnabled();
      if (debug)
         log.debug("restoring destination: "+dest);

      //remember this queue
      String queueName = dest.toString();
      LogInfo info = (LogInfo)messageLogs.get(queueName);
      if (info == null)
      {
         //must be new, set up directory etc.
         File logDir = new File(dataDir, encodeFileName(queueName));
         MessageLog msgLog = new MessageLog(messageCache, logDir);
         info = new LogInfo(msgLog, dest);
         synchronized (messageLogs)
         {
            messageLogs.put(queueName, info);
         }

      } // end of if ()
      else
      {
         info.destination = dest;
      } // end of else

      //restore the messages from old logs (previously read into unrestoredMessages)
      Map messages = (Map)unrestoredMessages.remove(queueName);
      if (messages != null)
      {
         if (debug)
           log.debug("Restore message count: "+messages.size());
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
               }
               jmsDest.restoreMessage(message);
            } // end of while ()

         }
      } // end of if ()
   }

   public void initQueue(SpyDestination dest) throws JMSException
   {
      try
      {
         File logDir = new File(dataDir, encodeFileName(dest.toString()));
         MessageLog log = new MessageLog(messageCache, logDir);
         LogInfo info = new LogInfo(log, dest);
         synchronized (messageLogs)
         {
            messageLogs.put(dest.toString(), info);
         }
      }
      catch (Exception e)
      {
         JMSException newE = new JMSException("Invalid configuration.");
         newE.setLinkedException(e);
         throw newE;
      }
   }

   /**
    *  #Description of the Method
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  JMSException  Description of Exception
    */
   public void add(MessageReference messageRef, org.jboss.mq.pm.Tx txId) throws JMSException
   {
   	  SpyMessage message = messageRef.getMessage();
      LogInfo logInfo;
      synchronized (messageLogs)
      {
         logInfo = (LogInfo)messageLogs.get(message.getJMSDestination().toString());
      }
      if (logInfo == null)
      {
         throw new JMSException("Destination was not initalized with the PersistenceManager");
      }
      logInfo.log.add(messageRef, txId);
      if (txId == null)
      {
         logInfo.log.finishAdd(messageRef, txId);
      }
      else
      {
         TxInfo info;
         synchronized (transactedTasks)
         {
            info = (TxInfo)transactedTasks.get(txId);
         }
         if (info == null)
         {
            throw new JMSException("Transaction is not active 5.");
         }
         synchronized (info.tasks)
         {
            info.tasks.addLast(new Transaction(true, logInfo, messageRef, txId));
         }
      }
   }

   /**
    *  #Description of the Method
    *
    * @param  txId                        Description of Parameter
    * @exception  JMSException  Description of Exception
    */
   public void commitPersistentTx(org.jboss.mq.pm.Tx txId) throws JMSException
   {
      TxInfo info;
      synchronized (transactedTasks)
      {
         info = (TxInfo)transactedTasks.remove(txId);
      }
      //ensure record of tx exists
      try
      {
         info.raf.close();
      }
      catch (IOException e)
      {
         JMSException jmse = new SpyJMSException("IO Error when closing raf for tx.");
         jmse.setLinkedException(e);
         throw jmse;
      }
      synchronized (info.tasks)
      {
         Iterator iter = info.tasks.iterator();
         while (iter.hasNext())
         {
            Transaction task = (Transaction)iter.next();
            task.commit();
         }
      }
      deleteTxFile(info.txf);
      releaseTxInfo(info);
   }

   /**
    *  #Description of the Method
    *
    * @return                             Description of the Returned Value
    * @exception  JMSException  Description of Exception
    */
   public org.jboss.mq.pm.Tx createPersistentTx() throws JMSException
   {
      org.jboss.mq.pm.Tx txId = null;
      synchronized (transactedTasks)
      {
         txId = new org.jboss.mq.pm.Tx(tidcounter++);
         transactedTasks.put(txId, getTxInfo(createTxFile(txId)));
      }
      return txId;
   }

   /**
    *  #Description of the Method
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  JMSException  Description of Exception
    */
   public void remove(MessageReference messageRef, org.jboss.mq.pm.Tx txId) throws JMSException
   {
      SpyMessage message = messageRef.getMessage();	 
      LogInfo logInfo;

      synchronized (messageLogs)
      {
         logInfo = (LogInfo)messageLogs.get(message.getJMSDestination().toString());
      }

      if (logInfo == null)
      {
         throw new JMSException("Destination was not initalized with the PersistenceManager");
      }

      logInfo.log.remove(message, txId);
      if (txId == null)
      {
         logInfo.log.finishRemove(messageRef, txId);
      }
      else
      {
         TxInfo info;
         synchronized (transactedTasks)
         {
            info = (TxInfo)transactedTasks.get(txId);
         }
         if (info == null)
         {
            throw new JMSException("Transaction is not active 6.");
         }
         try
         {
            info.raf.writeUTF(message.getJMSMessageID());
         }
         catch (IOException e)
         {
            JMSException jmse = new SpyJMSException("IO Error when recording remove in txs raf.");
            jmse.setLinkedException(e);
            throw jmse;
         }
         synchronized (info.tasks)
         {
            info.tasks.addLast(new Transaction(false, logInfo, messageRef, txId));
         }
      }

   }

   /**
    *  #Description of the Method
    *
    * @param  message                     Description of Parameter
    * @param  txId                        Description of Parameter
    * @exception  JMSException  Description of Exception
    */
   public void update(MessageReference messageRef, org.jboss.mq.pm.Tx txId) throws JMSException
   {
      SpyMessage message = messageRef.getMessage();	 
      LogInfo logInfo;

      synchronized (messageLogs)
      {
         logInfo = (LogInfo)messageLogs.get(message.getJMSDestination().toString());
      }

      if (logInfo == null)
      {
         throw new JMSException("Destination was not initalized with the PersistenceManager");
      }

      logInfo.log.update(messageRef, txId);
      if (txId == null)
      {
         logInfo.log.finishUpdate(messageRef, txId);
      }
      else
      {
         throw new JMSException("NYI: No code does transactional updates.");
      }

   }

   /**
    *  #Description of the Method
    *
    * @param  txId                        Description of Parameter
    * @exception  JMSException  Description of Exception
    */
   public void rollbackPersistentTx(org.jboss.mq.pm.Tx txId) throws JMSException
   {
      TxInfo info;
      synchronized (transactedTasks)
      {
         info = (TxInfo)transactedTasks.remove(txId);
      }
      //ensure record of tx exists
      try
      {
         info.raf.close();
      }
      catch (IOException e)
      {
         JMSException jmse = new SpyJMSException("IO Error when closing raf for tx.");
         jmse.setLinkedException(e);
         throw jmse;
      }
      synchronized (info.tasks)
      {
         Iterator iter = info.tasks.iterator();
         while (iter.hasNext())
         {
            Transaction task = (Transaction)iter.next();
            task.rollback();
         }
      }
      deleteTxFile(info.txf);
      releaseTxInfo(info);
   }

   protected TxInfo getTxInfo(File f) throws JMSException
   {
      TxInfo info;
      synchronized (txPool)
      {
         if (txPool.isEmpty())
         {
            info = new TxInfo();
         }
         else
         {
            info = (TxInfo)txPool.remove(txPool.size() - 1);
         }
      }
      info.setFile(f);
      return info;
   }

   protected void releaseTxInfo(TxInfo info)
   {
      synchronized (txPool)
      {
         if (txPool.size() < MAX_POOL_SIZE)
         {
            info.tasks.clear();
            txPool.add(info);
         }
      }
   }

   protected boolean testRollBackTx(Long tx, java.util.ArrayList removingMessages) throws IOException
   {
      //checks to see if this tx was in the middle of committing.
      //If it was finish commit and return false else return true.
      HashMap clone;
      synchronized (messageLogs)
      {
         clone = (HashMap)messageLogs.clone();
      }

      java.util.ArrayList files = new java.util.ArrayList();
      boolean foundAll = true;
      for (int i = 0; i < removingMessages.size(); i++)
      {
         String fileName = removingMessages.get(i) + "." + tx;
         boolean found = false;
         for (Iterator it = clone.keySet().iterator(); !found && it.hasNext(); )
         {
            String dirName = (String)it.next();
            File dir = new File(dataDir, encodeFileName(dirName));
            File[] messageFiles = dir.listFiles();
            for (int j = 0; j < messageFiles.length; ++j)
            {
               if (messageFiles[j].getName().equals(fileName))
               {
                  found = true;
                  files.add(messageFiles[j]);
                  break;
               }
            }
         }
         if (!found)
         {
            foundAll = false;
         }
      }
      if (!foundAll)
      {
         //tx being committed so need to finish it by deleting files.
         for (int i = 0; i < files.size(); ++i)
         {
            File f = (File)files.get(i);
            if (!f.delete())
            {
               Thread.yield();
               //try again
               if (!f.delete())
               {
                  throw new IOException("Could not delete file " + f.getAbsolutePath());
               }
            }
         }
         return false;
      }
      return true;
   }



   protected void deleteTxFile(File file) throws JMSException
   {
      if (!file.delete())
      {
         Thread.yield();
         if (file.exists() && !file.delete())
         {
            throw new JMSException("Unable to delete committing transaction record.");
         }
      }
   }

   protected ArrayList readTxFile(File file) throws JMSException
   {
      try
      {
         ArrayList result = new ArrayList();
         java.io.RandomAccessFile raf = new java.io.RandomAccessFile(file, "r");
         try
         {
            while (true)
            {
               result.add(raf.readUTF());
            }
         }
         catch (java.io.EOFException ignore) {}

         raf.close();
         return result;
      }
      catch (IOException e)
      {
         JMSException newE = new SpyJMSException("Unable to read committing transaction record.");
         newE.setLinkedException(e);
         throw newE;
      }
   }

   protected File createTxFile(org.jboss.mq.pm.Tx txId) throws JMSException
   {
      try
      {
         File file = new File(dataDir, txId.toString());
         if (!file.createNewFile())
         {
            throw new JMSException("Error creating tx file.");
         }
         return file;
      }
      catch (IOException e)
      {
         JMSException newE = new SpyJMSException("Unable to create committing transaction record.");
         newE.setLinkedException(e);
         throw newE;
      }
   }

   /**
    *  #Description of the Class
    */
   class TxInfo
   {
      File txf;
      java.io.RandomAccessFile raf;
      LinkedList tasks = new LinkedList();

      TxInfo() throws JMSException
      {
      }

      void setFile(File f) throws JMSException
      {
         txf = f;
         try
         {
            raf = new java.io.RandomAccessFile(txf, "rw");
         }
         catch (IOException e)
         {
            JMSException jmse = new SpyJMSException("IO Error create raf for txinfo.");
            jmse.setLinkedException(e);
            throw jmse;
         }
      }
   }

   /**
    *  #Description of the Class
    */
   class LogInfo
   {
      MessageLog log;
      SpyDestination destination;

      LogInfo(MessageLog log, SpyDestination destination)
      {
         this.log = log;
         this.destination = destination;
      }
   }





   /**
    *  #Description of the Class
    */
   class Transaction
   {
      private LogInfo logInfo;
      private MessageReference message;
      private org.jboss.mq.pm.Tx txId;
      private boolean add;

      /**
       *  Constructor for the Transaction object
       *
       * @param  add      Description of Parameter
       * @param  logInfo  Description of Parameter
       * @param  message  Description of Parameter
       * @param  txId     Description of Parameter
       */
      public Transaction(boolean add, LogInfo logInfo, MessageReference message, org.jboss.mq.pm.Tx txId)
      {
         this.add = add;
         this.logInfo = logInfo;
         this.message = message;
         this.txId = txId;
      }

      /**
       *  #Description of the Method
       *
       * @exception  JMSException  Description of Exception
       */
      public void commit() throws JMSException
      {
         if (add)
         {
            logInfo.log.finishAdd(message, txId);
         }
         else
         {
            logInfo.log.finishRemove(message, txId);
         }
      }

      /**
       *  #Description of the Method
       *
       * @exception  JMSException  Description of Exception
       */
      public void rollback() throws JMSException
      {
         if (add)
         {
            logInfo.log.undoAdd(message, txId);
         }
         else
         {
            logInfo.log.undoRemove(message, txId);
         }
      }
   }
   /*
    * @see PersistenceManager#closeQueue(JMSDestination, SpyDestination)
    */
   public void closeQueue(JMSDestination jmsDest, SpyDestination dest) throws JMSException
   {
      boolean debug = log.isDebugEnabled();
      if (debug)
         log.debug("closing destination: "+dest);

      // Cleanup all stuff we loaded/opened
      String queueName = dest.toString();
      LogInfo info = (LogInfo)messageLogs.remove(queueName);
      unrestoredMessages.remove(queueName);
   }
   /**
    * Used to encode any string into a string that is safe to use as 
    * a file name on most operating systems.
    */
   static public String encodeFileName(String name){
      NumberFormat nf = NumberFormat.getInstance();
      nf.setMinimumIntegerDigits(3);
      StringBuffer rc = new StringBuffer();
      for(int i=0; i< name.length(); i++ ) {
         switch( name.charAt(i) ) {
            // These are the safe characters...
            case 'a': case 'A': case 'b': case 'B': case 'c': case 'C':
            case 'd': case 'D': case 'e': case 'E': case 'f': case 'F':
            case 'g': case 'G': case 'h': case 'H': case 'i': case 'I':
            case 'j': case 'J': case 'k': case 'K': case 'l': case 'L':
            case 'm': case 'M': case 'n': case 'N': case 'o': case 'O':
            case 'p': case 'P': case 'q': case 'Q': case 'r': case 'R':
            case 's': case 'S': case 't': case 'T': case 'u': case 'U':
            case 'v': case 'V': case 'w': case 'W': case 'x': case 'X':
            case 'y': case 'Y': case 'z': case 'Z':
            case '1': case '2': case '3': case '4': case '5': 
            case '6': case '7': case '8': case '9': case '0': 
            case '-': case '_': case '.':
               rc.append(name.charAt(i));
               break;

            // Any other character needs to be encoded.
            default:
            
               // We encode the characters as %nnn where
               // nnn is the numeric value of the UTF8 byte of the character.
               // You might get %nnn%nnn since UTF8 can produce multiple
               // bytes for a since character.
               try {
                  byte data[] = (""+name.charAt(i)).getBytes("UTF8");
                  for( int j=0; j < data.length; j++  ) {
                     int t = ( 0 | data[j] );
                     rc.append('%');
                     rc.append( nf.format(t) );
                  }
               } catch (UnsupportedEncodingException wonthappen ){
               }
         }
      }
      return rc.toString();
   }
   

}
