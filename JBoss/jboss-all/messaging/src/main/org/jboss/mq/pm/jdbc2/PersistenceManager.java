/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.jdbc2;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.StreamCorruptedException;
import java.sql.Blob;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Properties;

import javax.jms.DeliveryMode;
import javax.jms.JMSException;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.sql.DataSource;
import javax.transaction.Status;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.mq.SpyDestination;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.pm.TxManager;
import org.jboss.mq.server.JMSDestination;
import org.jboss.mq.server.MessageCache;
import org.jboss.mq.server.MessageReference;
import org.jboss.mq.server.PersistentQueue;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.TransactionManagerService;

/**
 * This class manages all persistence related services for JDBC based
 * persistence.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean, org.jboss.mq.pm.PersistenceManagerMBean, org.jboss.mq.pm.CacheStoreMBean"
 *
 * @author Jayesh Parayali (jayeshpk1@yahoo.com)
 * @author Hiram Chirino (cojonudo14@hotmail.com)
 *
 *  @version $Revision: 1.6.2.12 $
 */
public class PersistenceManager
   extends ServiceMBeanSupport
   implements PersistenceManagerMBean, org.jboss.mq.pm.PersistenceManager, org.jboss.mq.pm.CacheStore
{

   /////////////////////////////////////////////////////////////////////////////////
   //
   // TX state attibutes
   //
   /////////////////////////////////////////////////////////////////////////////////
   private long nextTransactionId = 0;
   private TxManager txManager;
   private DataSource datasource;
   private TransactionManager tm;

   /////////////////////////////////////////////////////////////////////////////////
   //
   // JDBC Access Attributes
   //
   /////////////////////////////////////////////////////////////////////////////////

   String UPDATE_MARKED_MESSAGES = "UPDATE JMS_MESSAGES SET TXID=?, TXOP=? WHERE TXOP=?";
   String UPDATE_MARKED_MESSAGES_WITH_TX = "UPDATE JMS_MESSAGES SET TXID=?, TXOP=? WHERE TXOP=? AND TXID=?";
   String DELETE_MARKED_MESSAGES_WITH_TX = "DELETE FROM JMS_MESSAGES WHERE TXID IN (SELECT TXID FROM JMS_TRANSACTIONS) AND TXOP=?";
   String DELETE_TX = "DELETE FROM JMS_TRANSACTIONS WHERE TXID = ?";
   String DELETE_MARKED_MESSAGES = "DELETE FROM JMS_MESSAGES WHERE TXID=? AND TXOP=?";
   String INSERT_TX = "INSERT INTO JMS_TRANSACTIONS (TXID) values(?)";
   String SELECT_MAX_TX = "SELECT MAX(TXID) FROM JMS_MESSAGES";
   String SELECT_MESSAGES_IN_DEST = "SELECT MESSAGEID, MESSAGEBLOB FROM JMS_MESSAGES WHERE DESTINATION=?";
   String SELECT_MESSAGE = "SELECT MESSAGEID, MESSAGEBLOB FROM JMS_MESSAGES WHERE MESSAGEID=? AND DESTINATION=?";
   String INSERT_MESSAGE = "INSERT INTO JMS_MESSAGES (MESSAGEID, DESTINATION, MESSAGEBLOB, TXID, TXOP) VALUES(?,?,?,?,?)";
   String MARK_MESSAGE = "UPDATE JMS_MESSAGES SET TXID=?, TXOP=? WHERE MESSAGEID=? AND DESTINATION=?";
   String DELETE_MESSAGE = "DELETE FROM JMS_MESSAGES WHERE MESSAGEID=? AND DESTINATION=?";
   String UPDATE_MESSAGE = "UPDATE JMS_MESSAGES SET MESSAGEBLOB=? WHERE MESSAGEID=? AND DESTINATION=?";
   String CREATE_MESSAGE_TABLE =
      "CREATE TABLE JMS_MESSAGES ( MESSAGEID INTEGER NOT NULL, "
         + "DESTINATION VARCHAR(32) NOT NULL, TXID INTEGER, TXOP CHAR(1),"
         + "MESSAGEBLOB OBJECT, PRIMARY KEY (MESSAGEID, DESTINATION) )";
   String CREATE_TX_TABLE = "CREATE TABLE JMS_TRANSACTIONS ( TXID INTEGER )";

   static final int OBJECT_BLOB = 0;
   static final int BYTES_BLOB = 1;
   static final int BINARYSTREAM_BLOB = 2;
   static final int BLOB_BLOB = 3;

   int blobType = OBJECT_BLOB;
   boolean createTables;
   
   private int connectionRetryAttempts = 5;

   /////////////////////////////////////////////////////////////////////////////////
   //
   // Constructor.
   //
   /////////////////////////////////////////////////////////////////////////////////
   public PersistenceManager() throws javax.jms.JMSException
   {
      txManager = new TxManager(this);
   }

   /**
    * This inner class helps handle the tx management of the jdbc connections.
    * 
    */
   class TransactionManagerStrategy
   {

      Transaction threadTx;

      void startTX() throws JMSException
      {
         //log.debug("starting a new TM transaction");
         try
         {
            // Thread arriving must be clean (jboss doesn't set the thread
            // previously). However optimized calls come with associated
            // thread for example. We suspend the thread association here, and
            // resume in the finally block of the following try.
            threadTx = tm.suspend();

            // Always begin a transaction
            tm.begin();
         }
         catch (Exception e)
         {
            try
            {
               if (threadTx != null)
                  tm.resume(threadTx);
            }
            catch (Exception ignore)
            {
            }
            throw new SpyJMSException("Could not start a transaction with the transaction manager.", e);
         }
      }

      void setRollbackOnly() throws JMSException
      {
         //log.debug("rolling back a TM transaction");
         try
         {
            tm.setRollbackOnly();
         }
         catch (Exception e)
         {
            throw new SpyJMSException("Could not start a mark the transaction for rollback .", e);
         }
      }

      void endTX() throws JMSException
      {
         //log.debug("ending TM transaction.");
         try
         {
            if (tm.getStatus() == Status.STATUS_MARKED_ROLLBACK)
            {
               tm.rollback();
            }
            else
            {
               tm.commit();
            }
         }
         catch (Exception e)
         {
            throw new SpyJMSException("Could not start a transaction with the transaction manager.", e);
         }
         finally
         {
            try
            {
               if (threadTx != null)
                  tm.resume(threadTx);
            }
            catch (Exception ignore)
            {
            }
         }
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // TX Resolution.
   //
   /////////////////////////////////////////////////////////////////////////////////
   synchronized public void resolveAllUncommitedTXs() throws JMSException
   {
      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      ResultSet rs = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {
         if (createTables)
         {
            c = this.getConnection();

            try
            {
               stmt = c.prepareStatement(CREATE_MESSAGE_TABLE);
               stmt.executeUpdate();
            }
            catch (SQLException e)
            {
               log.debug("Could not create table with SQL: " + CREATE_MESSAGE_TABLE + ", got : " + e);
            }
            finally
            {
               try
               {
                  if (stmt != null)
                     stmt.close();
               }
               catch (Throwable ignored)
               {
                  log.trace("Ignored: " + ignored);
               }
               stmt = null;
            }

            try
            {
               stmt = c.prepareStatement(CREATE_TX_TABLE);
               stmt.executeUpdate();
            }
            catch (SQLException e)
            {
               log.debug("Could not create table with SQL: " + CREATE_TX_TABLE + ", got : " + e);
            }
            finally
            {
               try
               {
                  if (stmt != null)
                     stmt.close();
               }
               catch (Throwable ignored)
               {
                  log.trace("Ignored: " + ignored);
               }
               stmt = null;
            }
         }
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not get a connection for jdbc2 table construction ", e);
      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         stmt = null;
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         c = null;
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }

      // We perform recovery in a different thread to the table creation
      // Postgres doesn't like create table failing in the same transaction
      // as other operations

      tms = new TransactionManagerStrategy();
      tms.startTX();
      threadWasInterrupted = Thread.interrupted();
      try
      {
         c = this.getConnection();

         // Delete all the messages that were added but thier tx's were not commited.
         stmt = c.prepareStatement(DELETE_MARKED_MESSAGES_WITH_TX);
         stmt.setString(1, "A");
         stmt.executeUpdate();
         stmt.close();

         // Restore all the messages that were removed but their tx's were not commited.
         stmt = c.prepareStatement(UPDATE_MARKED_MESSAGES);
         stmt.setNull(1, java.sql.Types.BIGINT);
         stmt.setString(2, "A");
         stmt.setString(3, "D");
         stmt.executeUpdate();
         stmt.close();

         // Delete all the non persistent messages that were added by the 
         // CacheStore interface of this PM
         removeMarkedMessages(c, null, "T");

         // Find out what the next TXID should be
         stmt = c.prepareStatement(SELECT_MAX_TX);
         rs = stmt.executeQuery();
         if (rs.next())
         {
            nextTransactionId = rs.getLong(1) + 1;
         }

      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not resolve uncommited transactions.  Message recovery may not be accurate", e);
      }
      finally
      {
         try
         {
            rs.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // Message Recovery
   //
   /////////////////////////////////////////////////////////////////////////////////
   synchronized public void restoreQueue(JMSDestination jmsDest, SpyDestination dest) throws javax.jms.JMSException
   {
      if (jmsDest == null)
         throw new IllegalArgumentException("Must supply non null JMSDestination to restoreQueue");
      if (dest == null)
         throw new IllegalArgumentException("Must supply non null SpyDestination to restoreQueue");

      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      ResultSet rs = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         stmt = c.prepareStatement(SELECT_MESSAGES_IN_DEST);
         stmt.setString(1, dest.toString());

         rs = stmt.executeQuery();
         int counter=0;
         while (rs.next())
         {
            SpyMessage message = extractMessage(rs);
            // The durable subscription is not serialized
            if (dest instanceof SpyTopic)
               message.header.durableSubscriberID = ((SpyTopic)dest).getDurableSubscriptionID();
            jmsDest.restoreMessage(message);
            counter++;
         }
         
         log.debug("Restored "+counter+" message(s) to: "+dest);
      }
      catch (IOException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not restore messages to destination : " + dest.toString(), e);
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not restore messages to destination : " + dest.toString(), e);
      }
      finally
      {
         try
         {
            rs.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }

   }

   SpyMessage extractMessage(ResultSet rs) throws SQLException, IOException
   {
      try
      {
         long messageid = rs.getLong(1);

         SpyMessage message = null;

         if (blobType == OBJECT_BLOB)
         {

            message = (SpyMessage) rs.getObject(2);

         }
         else if (blobType == BYTES_BLOB)
         {

            byte[] st = rs.getBytes(2);
            ByteArrayInputStream baip = new ByteArrayInputStream(st);
            ObjectInputStream ois = new ObjectInputStream(baip);
            message = SpyMessage.readMessage(ois);

         }
         else if (blobType == BINARYSTREAM_BLOB)
         {

            ObjectInputStream ois = new ObjectInputStream(rs.getBinaryStream(2));
            message = SpyMessage.readMessage(ois);

         }
         else if (blobType == BLOB_BLOB)
         {

            ObjectInputStream ois = new ObjectInputStream(rs.getBlob(2).getBinaryStream());
            message = SpyMessage.readMessage(ois);
         }

         message.header.messageId = messageid;
         return message;
      }
      catch (StreamCorruptedException e)
      {
         throw new IOException("Could not load the message: " + e);
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // TX Commit
   //
   /////////////////////////////////////////////////////////////////////////////////
   public void commitPersistentTx(org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException
   {

      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         removeMarkedMessages(c, txId, "D");
         removeTXRecord(c, txId.longValue());

      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not commit tx: " + txId, e);
      }
      finally
      {
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
   }

   public void removeMarkedMessages(Connection c, org.jboss.mq.pm.Tx txid, String mark) throws SQLException
   {
      PreparedStatement stmt = null;
      try
      {
         stmt = c.prepareStatement(DELETE_MARKED_MESSAGES);
         if (txid != null)
            stmt.setLong(1, txid.longValue());
         else
            stmt.setNull(1, java.sql.Types.BIGINT);
         stmt.setString(2, mark);
         stmt.executeUpdate();
      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable e)
         {
         }
      }
   }

   public void removeTXRecord(Connection c, long txid) throws SQLException
   {
      PreparedStatement stmt = null;
      try
      {
         stmt = c.prepareStatement(DELETE_TX);
         stmt.setLong(1, txid);
         stmt.executeUpdate();
      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable e)
         {
         }
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // TX Rollback
   //
   /////////////////////////////////////////////////////////////////////////////////
   public void rollbackPersistentTx(org.jboss.mq.pm.Tx txId) throws JMSException
   {

      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         removeMarkedMessages(c, txId, "A");
         removeTXRecord(c, txId.longValue());

         // Restore all the messages that were logically removed.
         stmt = c.prepareStatement(UPDATE_MARKED_MESSAGES_WITH_TX);
         stmt.setNull(1, java.sql.Types.BIGINT);
         stmt.setString(2, "A");
         stmt.setString(3, "D");
         stmt.setLong(4, txId.longValue());
         stmt.executeUpdate();
         stmt.close();

      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not rollback tx: " + txId, e);
      }
      finally
      {
         try
         {
            if (stmt != null)
               stmt.close();
            if (c != null)
               c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }

   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // TX Creation
   //
   /////////////////////////////////////////////////////////////////////////////////
   public org.jboss.mq.pm.Tx createPersistentTx() throws JMSException
   {

      org.jboss.mq.pm.Tx id = new org.jboss.mq.pm.Tx(nextTransactionId++);
      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         stmt = c.prepareStatement(INSERT_TX);
         stmt.setLong(1, id.longValue());
         stmt.executeUpdate();

      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not crate tx: " + id, e);
      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }

      return id;
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // Adding a message
   //
   /////////////////////////////////////////////////////////////////////////////////
   public void add(MessageReference messageRef, org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("About to add message " + messageRef + " transaction=" + txId);

      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {
         c = this.getConnection();
         // Synchronize on the message to avoid a race with the softener
         synchronized(messageRef)
         {
            SpyMessage message = messageRef.getMessage();

            // has it allready been stored by the message cache interface??
            if (messageRef.stored == MessageReference.STORED)
            {
               if (trace)
                  log.trace("Updating message " + messageRef + " transaction=" + txId);

               markMessage(c, messageRef.messageId, messageRef.getPersistentKey(), txId, "A");
            }
            else
            {
               if (trace)
                  log.trace("Inserting message " + messageRef + " transaction=" + txId);

               add(c, messageRef.getPersistentKey(), message, txId, "A");
               messageRef.setStored(MessageReference.STORED);
            }
            if (trace)
               log.trace("Added message " + messageRef + " transaction=" + txId);
         }
      }
      catch (IOException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not store message: " + messageRef, e);
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not store message: " + messageRef, e);
      }
      finally
      {
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
   }

   protected void add(Connection c, String queue, SpyMessage message, org.jboss.mq.pm.Tx txId, String mark)
      throws SQLException, IOException
   {
      PreparedStatement stmt = null;
      try
      {

         stmt = c.prepareStatement(INSERT_MESSAGE);

         stmt.setLong(1, message.header.messageId);
         stmt.setString(2, queue);
         setBlob(stmt, 3, message);

         if (txId != null)
            stmt.setLong(4, txId.longValue());
         else
            stmt.setNull(4, java.sql.Types.BIGINT);
         stmt.setString(5, mark);

         stmt.executeUpdate();

      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
      }
   }

   public void markMessage(Connection c, long messageid, String destination, org.jboss.mq.pm.Tx txId, String mark)
      throws SQLException
   {
      PreparedStatement stmt = null;
      try
      {

         stmt = c.prepareStatement(MARK_MESSAGE);
         if (txId == null)
         {
            stmt.setNull(1, java.sql.Types.BIGINT);
         }
         else
         {
            stmt.setLong(1, txId.longValue());
         }
         stmt.setString(2, mark);
         stmt.setLong(3, messageid);
         stmt.setString(4, destination);
         stmt.executeUpdate();

      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
      }

   }

   public void setBlob(PreparedStatement stmt, int column, SpyMessage message)
      throws IOException, SQLException
   {
         if (blobType == OBJECT_BLOB)
         {
            stmt.setObject(column, message);
         }
         else if (blobType == BYTES_BLOB)
         {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            ObjectOutputStream oos = new ObjectOutputStream(baos);
            SpyMessage.writeMessage(message,oos);
            oos.flush();
            byte[] messageAsBytes = baos.toByteArray();
            stmt.setBytes(column, messageAsBytes);
         }
         else if (blobType == BINARYSTREAM_BLOB)
         {
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            ObjectOutputStream oos = new ObjectOutputStream(baos);
            SpyMessage.writeMessage(message,oos);
            oos.flush();
            byte[] messageAsBytes = baos.toByteArray();
            ByteArrayInputStream bais = new ByteArrayInputStream(messageAsBytes);
            stmt.setBinaryStream(column, bais, messageAsBytes.length);
         }
         else if (blobType == BLOB_BLOB)
         {
            
            throw new RuntimeException("BLOB_TYPE: BLOB_BLOB is not yet implemented.");
            /** TODO:
            ByteArrayOutputStream baos= new ByteArrayOutputStream();
            ObjectOutputStream oos= new ObjectOutputStream(baos);
            oos.writeObject(message);
            byte[] messageAsBytes= baos.toByteArray();
            ByteArrayInputStream bais= new ByteArrayInputStream(messageAsBytes);
            stmt.setBsetBinaryStream(column, bais, messageAsBytes.length);
            */
         }
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // Updating a message
   //
   /////////////////////////////////////////////////////////////////////////////////
   public void update(MessageReference messageRef, org.jboss.mq.pm.Tx txId)
      throws javax.jms.JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("Updating message " + messageRef + " transaction=" + txId);

      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         if (txId == null)
         {

            stmt = c.prepareStatement(UPDATE_MESSAGE);
            setBlob(stmt, 1, messageRef.getMessage());
            stmt.setLong(2, messageRef.messageId);
            stmt.setString(3, messageRef.getPersistentKey());
            int rc = stmt.executeUpdate();
            if(  rc != 1 ) 
               throw new SpyJMSException("Could not update the message in the database: update affected "+rc+" rows");
         }
         else
         {
            throw new SpyJMSException("NYI: Updating a message in a transaction is not currently used");
         }
         if (trace)
            log.trace("Updated message " + messageRef + " transaction=" + txId);

      }
      catch (IOException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not update message: " + messageRef, e);
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not update message: " + messageRef, e);
      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }

   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // Removing a message
   //
   /////////////////////////////////////////////////////////////////////////////////
   public void remove(MessageReference messageRef, org.jboss.mq.pm.Tx txId) throws javax.jms.JMSException
   {
      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("Removing message " + messageRef + " transaction=" + txId);

      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         // Synchronize on the message to avoid a race with the softener
         synchronized(messageRef)
         {
            if (txId == null)
            {
               stmt = c.prepareStatement(DELETE_MESSAGE);
               stmt.setLong(1, messageRef.messageId);
               stmt.setString(2, messageRef.getPersistentKey());
               int rc = stmt.executeUpdate();
               if(  rc != 1 ) 
                  throw new SpyJMSException("Could not delete the message from the database: delete affected "+rc+" rows");

               // Adrian Brock:
               // Remove the message from the cache, but don't 
               // return it to the pool just yet. The queue still holds
               // a reference to the message and will return it
               // to the pool once it gets enough time slice.
               // The alternative is to remove the validation
               // for double removal from the cache, 
               // which I don't want to do because it is useful
               // for spotting errors
               messageRef.setStored(MessageReference.NOT_STORED);
               messageRef.removeDelayed();
            }
            else
            {
               stmt = c.prepareStatement(MARK_MESSAGE);
               stmt.setLong(1, txId.longValue());
               stmt.setString(2, "D");
               stmt.setLong(3, messageRef.messageId);
               stmt.setString(4, messageRef.getPersistentKey());
               int rc = stmt.executeUpdate();
               if(  rc != 1 )
                  throw new SpyJMSException("Could not mark the message as deleted in the database: update affected "+rc+" rows");
            }
            if (trace)
               log.trace("Removed message " + messageRef + " transaction=" + txId);
         }
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not remove message: " + messageRef, e);
      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }

   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // Misc. PM functions
   //
   /////////////////////////////////////////////////////////////////////////////////

   public org.jboss.mq.pm.TxManager getTxManager()
   {
      return txManager;
   }

   /*
    * @see PersistenceManager#closeQueue(JMSDestination, SpyDestination)
    */
   public void closeQueue(JMSDestination jmsDest, SpyDestination dest) throws JMSException
   {
      // Nothing to clean up, all the state is in the db.
   }

   /*
    * @see CacheStore#loadFromStorage(MessageReference)
    */
   public SpyMessage loadFromStorage(MessageReference messageRef) throws JMSException
   {
      if (log.isTraceEnabled())
         log.trace("Loading message from storage " + messageRef);

      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      ResultSet rs = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         stmt = c.prepareStatement(SELECT_MESSAGE);
         stmt.setLong(1, messageRef.messageId);
         stmt.setString(2, messageRef.getPersistentKey());

         rs = stmt.executeQuery();
         if (rs.next())
            return extractMessage(rs);

         return null;

      }
      catch (IOException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not load message : " + messageRef, e);
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not load message : " + messageRef, e);
      }
      finally
      {
         try
         {
            rs.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // CacheStore Functions
   //
   /////////////////////////////////////////////////////////////////////////////////   
   public void removeFromStorage(MessageReference messageRef) throws JMSException
   {
      // We don't remove persistent messages sent to persistent queues
      if (messageRef.isPersistent())
         return;   

      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("Removing message from storage " + messageRef);
      
      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      PreparedStatement stmt = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {
         c = this.getConnection();
         stmt = c.prepareStatement(DELETE_MESSAGE);
         stmt.setLong(1, messageRef.messageId);
         stmt.setString(2, messageRef.getPersistentKey());
         stmt.executeUpdate();
         messageRef.setStored(MessageReference.NOT_STORED);

         if (trace)
            log.trace("Removed message from storage " + messageRef);
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not remove message: " + messageRef, e);
      }
      finally
      {
         try
         {
            stmt.close();
         }
         catch (Throwable ignore)
         {
         }
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
   }

   /*
    * @see CacheStore#saveToStorage(MessageReference, SpyMessage)
    */
   public void saveToStorage(MessageReference messageRef, SpyMessage message) throws JMSException
   {
      // Ignore save operations for persistent messages sent to persistent queues
      // The queues handle the persistence
      if (messageRef.isPersistent())
         return;

      boolean trace = log.isTraceEnabled();
      if (trace)
         log.trace("Saving message to storage " + messageRef);
      
      TransactionManagerStrategy tms = new TransactionManagerStrategy();
      tms.startTX();
      Connection c = null;
      boolean threadWasInterrupted = Thread.interrupted();
      try
      {

         c = this.getConnection();
         add(c, messageRef.getPersistentKey(), message, null, "T");
         messageRef.setStored(MessageReference.STORED);

         if (trace)
            log.trace("Saved message to storage " + messageRef);
      }
      catch (IOException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not store message: " + messageRef, e);
      }
      catch (SQLException e)
      {
         tms.setRollbackOnly();
         throw new SpyJMSException("Could not store message: " + messageRef, e);
      }
      finally
      {
         try
         {
            c.close();
         }
         catch (Throwable ignore)
         {
         }
         tms.endTX();

         // Restore the interrupted state of the thread
         if( threadWasInterrupted )
            Thread.currentThread().interrupt();
      }
   }
   
   /**
    * Gets a connection from the datasource, retrying as needed.  This was
    * implemented because in some minimal configurations (i.e. little logging
    * and few services) the database wasn't ready when we tried to get a
    * connection.  We, therefore, implement a retry loop wich is controled
    * by the ConnectionRetryAttempts attribute.  Submitted by terry@amicas.com
    *
    * @exception SQLException if an error occurs.
    */
   private Connection getConnection() throws SQLException
   {
       int attempts = this.connectionRetryAttempts;
       int attemptCount = 0;
       SQLException sqlException = null;
       while (attempts-- > 0)
       {
           if (++attemptCount > 1)
           {
               log.debug("Retrying connection: attempt # " + attemptCount);
           }
           try
           {
               sqlException = null;
               return datasource.getConnection();
           }
           catch (SQLException exception)
           {
               log.debug("Connection attempt # " + attemptCount + " failed with SQLException", exception);
               sqlException = exception;
           }
           finally
           {
               if (sqlException == null && attemptCount > 1)
               {
                   log.debug("Connection succeeded on attempt # " + attemptCount);
               }
           }
           
           if (attempts > 0)
           {
               try
               {
                   Thread.sleep(1500);
               }
               catch(InterruptedException interruptedException)
               {
                   break;
               }
           }
       }
       if (sqlException != null)
       {
           throw sqlException;
       }
       throw new SQLException("connection attempt interrupted");
   }

   /////////////////////////////////////////////////////////////////////////////////
   //
   // JMX Interface 
   //
   /////////////////////////////////////////////////////////////////////////////////   
   private ObjectName connectionManagerName;
   private Properties sqlProperties = new Properties();

   public void startService() throws Exception
   {
      UPDATE_MARKED_MESSAGES = sqlProperties.getProperty("UPDATE_MARKED_MESSAGES", UPDATE_MARKED_MESSAGES);
      UPDATE_MARKED_MESSAGES_WITH_TX = sqlProperties.getProperty("UPDATE_MARKED_MESSAGES_WITH_TX", UPDATE_MARKED_MESSAGES_WITH_TX);
      DELETE_MARKED_MESSAGES_WITH_TX = sqlProperties.getProperty("DELETE_MARKED_MESSAGES_WITH_TX", DELETE_MARKED_MESSAGES_WITH_TX);
      DELETE_TX = sqlProperties.getProperty("DELETE_TX", DELETE_TX);
      DELETE_MARKED_MESSAGES = sqlProperties.getProperty("DELETE_MARKED_MESSAGES", DELETE_MARKED_MESSAGES);
      INSERT_TX = sqlProperties.getProperty("INSERT_TX", INSERT_TX);
      SELECT_MAX_TX = sqlProperties.getProperty("SELECT_MAX_TX", SELECT_MAX_TX);
      SELECT_MESSAGES_IN_DEST = sqlProperties.getProperty("SELECT_MESSAGES_IN_DEST", SELECT_MESSAGES_IN_DEST);
      SELECT_MESSAGE = sqlProperties.getProperty("SELECT_MESSAGE", SELECT_MESSAGE);
      INSERT_MESSAGE = sqlProperties.getProperty("INSERT_MESSAGE", INSERT_MESSAGE);
      MARK_MESSAGE = sqlProperties.getProperty("MARK_MESSAGE", MARK_MESSAGE);
      DELETE_MESSAGE = sqlProperties.getProperty("DELETE_MESSAGE", DELETE_MESSAGE);
      UPDATE_MESSAGE = sqlProperties.getProperty("UPDATE_MESSAGE", UPDATE_MESSAGE);
      CREATE_MESSAGE_TABLE = sqlProperties.getProperty("CREATE_MESSAGE_TABLE", CREATE_MESSAGE_TABLE);
      CREATE_TX_TABLE = sqlProperties.getProperty("CREATE_TX_TABLE", CREATE_TX_TABLE);
      createTables = sqlProperties.getProperty("CREATE_TABLES_ON_STARTUP", "true").equalsIgnoreCase("true");
      String s = sqlProperties.getProperty("BLOB_TYPE", "OBJECT_BLOB");

      if (s.equals("OBJECT_BLOB"))
      {
         blobType = OBJECT_BLOB;
      }
      else if (s.equals("BYTES_BLOB"))
      {
         blobType = BYTES_BLOB;
      }
      else if (s.equals("BINARYSTREAM_BLOB"))
      {
         blobType = BINARYSTREAM_BLOB;
      }
      else if (s.equals("BLOB_BLOB"))
      {
         blobType = BLOB_BLOB;
      }

      //Find the ConnectionFactoryLoader MBean so we can find the datasource
      String dsName = (String) getServer().getAttribute(connectionManagerName, "JndiName");
      //Get an InitialContext

      InitialContext ctx = new InitialContext();
      datasource = (DataSource) ctx.lookup("java:/" + dsName);

      //Get the Transaction Manager so we can control the jdbc tx
      tm = (TransactionManager) ctx.lookup(TransactionManagerService.JNDI_NAME);

      log.debug("Resolving uncommited TXS");
      resolveAllUncommitedTXs();
   }

   /**
    * Describe <code>getInstance</code> method here.
    *
    * @return an <code>Object</code> value
    * @jmx:managed-attribute
    */
   public Object getInstance()
   {
      return this;
   }

   /**
    * Describe <code>getMessageCache</code> method here.
    *
    * @return an <code>ObjectName</code> value
    */
   public ObjectName getMessageCache()
   {
      throw new UnsupportedOperationException("This is now set on the destination manager");
   }

   /**
    * Describe <code>setMessageCache</code> method here.
    *
    * @param messageCache an <code>ObjectName</code> value
    */
   public void setMessageCache(ObjectName messageCache)
   {
      throw new UnsupportedOperationException("This is now set on the destination manager");
   }

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getConnectionManager()
   {
      return connectionManagerName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setConnectionManager(ObjectName connectionManagerName)
   {
      this.connectionManagerName = connectionManagerName;
   }

   public MessageCache getMessageCacheInstance()
   {
      throw new UnsupportedOperationException("This is now set on the destination manager");
   }

   /**
    * Gets the sqlProperties.
    *
    * @jmx:managed-attribute
    *
    * @return Returns a Properties
    */
   public String getSqlProperties()
   {
      try
      {
         ByteArrayOutputStream boa = new ByteArrayOutputStream();
         sqlProperties.store(boa, "");
         return new String(boa.toByteArray());
      }
      catch (IOException shouldnothappen)
      {
         return "";
      }
   }

   /**
    * Sets the sqlProperties.
    *
    * @jmx:managed-attribute
    *
    * @param sqlProperties The sqlProperties to set
    */
   public void setSqlProperties(String value)
   {
      try
      {

         ByteArrayInputStream is = new ByteArrayInputStream(value.getBytes());
         sqlProperties = new Properties();
         sqlProperties.load(is);

      }
      catch (IOException shouldnothappen)
      {
      }
   }
   
   /**
    * Sets the ConnectionRetryAttempts.
    *
    * @jmx:managed-attribute
    *
    * @param connectionRetryAttempts value
    */
   public void setConnectionRetryAttempts(int value)
   {
       this.connectionRetryAttempts = value;
   }
   
   /**
    * Gets the ConnectionRetryAttempts.
    *
    * @jmx:managed-attribute
    *
    * @return Returns a ConnectionRetryAttempt value
    */
   public int getConnectionRetryAttempts()
   {
       return this.connectionRetryAttempts;
   }
   
}
