/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.sm.jdbc;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Properties;

import javax.jms.InvalidClientIDException;
import javax.jms.JMSException;
import javax.jms.JMSSecurityException;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.sql.DataSource;
import javax.transaction.Status;
import javax.transaction.Transaction;
import javax.transaction.TransactionManager;

import org.jboss.logging.Logger;
import org.jboss.mq.DurableSubscriptionID;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyTopic;
import org.jboss.mq.sm.AbstractStateManager;
import org.jboss.mq.sm.StateManager;
import org.jboss.tm.TransactionManagerService;

/**
 * A state manager that stores state in the database.
 *
 * @jmx:mbean extends="org.jboss.mq.sm.AbstractStateManagerMBean"
 *
 * @todo test it (passes the testsuite, except LargeMessage throws OutOfMemory?)
 * @todo create configurations (login module is just DatabaseServerLoginModule)
 * @todo add support for jmx operations to maintain the database
 * @todo create indices
 *
 * @author     Adrian Brock (Adrian@jboss.org)
 * @version    $Revision: 1.1.2.1 $
 */
public class JDBCStateManager
   extends AbstractStateManager
   implements JDBCStateManagerMBean
{
   // Constants -----------------------------------------------------

   static final Logger log = Logger.getLogger(JDBCStateManager.class);

   // Attributes ----------------------------------------------------

   /** The connection manager */
   private ObjectName connectionManagerName;

   /** The data source */
   private DataSource dataSource;

   /** Whether there is a security manager */
   private boolean hasSecurityManager = true;

   /** The transaction manager */
   private TransactionManager tm;

   /** The sql properties */
   private Properties sqlProperties = new Properties();

   /** Whether to create tables */
   private boolean createTables = true;

   /** Create the user table */
   private String CREATE_USER_TABLE = 
      "CREATE TABLE JMS_USERS (USERID VARCHAR(32) NOT NULL, PASSWD VARCHAR(32) NOT NULL, CLIENTID VARCHAR(128)," +
      " PRIMARY KEY(USERID))";
   /** Create the role table */
   private String CREATE_ROLE_TABLE = 
      "CREATE TABLE JMS_ROLES (ROLEID VARCHAR(32) NOT NULL, USERID VARCHAR(32) NOT NULL," +
      " PRIMARY KEY(USERID, ROLEID))";
   private String CREATE_SUBSCRIPTION_TABLE = 
      "CREATE TABLE JMS_SUBSCRIPTIONS (CLIENTID VARCHAR(128) NOT NULL, NAME VARCHAR(128) NOT NULL," +
      " TOPIC VARCHAR(255) NOT NULL, SELECTOR VARCHAR(255)," +
      " PRIMARY KEY(CLIENTID, NAME))";
   /** Get a subscription */
   private String GET_SUBSCRIPTION = 
      "SELECT TOPIC, SELECTOR FROM JMS_SUBSCRIPTIONS WHERE CLIENTID=? AND NAME=?";
   /** Get subscriptions for a topic */
   private String GET_SUBSCRIPTIONS_FOR_TOPIC = 
      "SELECT CLIENTID, NAME, SELECTOR FROM JMS_SUBSCRIPTIONS WHERE TOPIC=?";
   /** Lock a subscription */
   private String LOCK_SUBSCRIPTION = 
      "SELECT TOPIC, SELECTOR FROM JMS_SUBSCRIPTIONS WHERE CLIENTID=? AND NAME=?";
   /** Insert a subscription */
   private String INSERT_SUBSCRIPTION = 
      "INSERT INTO JMS_SUBSCRIPTIONS (CLIENTID, NAME, TOPIC, SELECTOR) VALUES(?,?,?,?)";
   /** Update a subscription */
   private String UPDATE_SUBSCRIPTION = 
      "UPDATE JMS_SUBSCRIPTIONS SET TOPIC=?, SELECTOR=? WHERE CLIENTID=? AND NAME=?";
   /** Remove a subscription */
   private String REMOVE_SUBSCRIPTION = 
      "DELETE FROM JMS_SUBSCRIPTIONS WHERE CLIENTID=? AND NAME=?";
   /** Get a user with the given client id */
   private String GET_USER_BY_CLIENTID = 
      "SELECT USERID, PASSWD, CLIENTID FROM JMS_USERS WHERE CLIENTID=?";
   /** Get a user with the given user id */
   private String GET_USER = 
      "SELECT PASSWD, CLIENTID FROM JMS_USERS WHERE USERID=?";

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   // Public --------------------------------------------------------

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

   /**
    * @jmx:managed-attribute
    */
   public boolean hasSecurityManager()
   {
      return hasSecurityManager;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setHasSecurityManager(boolean hasSecurityManager)
   {
      this.hasSecurityManager = hasSecurityManager;
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

   // AbstractStateManager overrides --------------------------------

   protected DurableSubscription getDurableSubscription(DurableSubscriptionID sub)
      throws JMSException
   {
      JDBCSession session = new JDBCSession();
      try
      {
         PreparedStatement statement = session.prepareStatement(GET_SUBSCRIPTION);
         statement.setString(1, sub.getClientID());
         statement.setString(2, sub.getSubscriptionName());
         ResultSet rs = statement.executeQuery();
         if (rs.next() == false)
            return null;

         return new DurableSubscription(sub.getClientID(), sub.getSubscriptionName(), rs.getString(1), rs.getString(2));
      }
      catch (SQLException e)
      {
         session.setRollbackOnly();
         throw new SpyJMSException("Error getting durable subscription " + sub, e);
      }
      finally
      {
         session.close();
      }      
   }

   protected void saveDurableSubscription(DurableSubscription ds)
      throws JMSException
   {
      JDBCSession session = new JDBCSession();
      try
      {
         PreparedStatement statement = session.prepareStatement(LOCK_SUBSCRIPTION);
         statement.setString(1, ds.getClientID());
         statement.setString(2, ds.getName());
         ResultSet rs = statement.executeQuery();
         if (rs.next() == false)
         {
            statement = session.prepareStatement(INSERT_SUBSCRIPTION);
            statement.setString(1, ds.getClientID());
            statement.setString(2, ds.getName());
            statement.setString(3, ds.getTopic());
            statement.setString(4, ds.getSelector());
         }
         else
         {
            statement = session.prepareStatement(UPDATE_SUBSCRIPTION);
            statement.setString(1, ds.getTopic());
            statement.setString(2, ds.getSelector());
            statement.setString(3, ds.getClientID());
            statement.setString(4, ds.getName());
         }
         if (statement.executeUpdate() != 1)
         {
            session.setRollbackOnly();
            throw new SpyJMSException("Insert subscription failed " + ds);
         }
      }
      catch (SQLException e)
      {
         session.setRollbackOnly();
         throw new SpyJMSException("Error saving durable subscription " + ds, e);
      }
      finally
      {
         session.close();
      }      
   }

   protected void removeDurableSubscription(DurableSubscription ds)
      throws JMSException
   {
      JDBCSession session = new JDBCSession();
      try
      {
         PreparedStatement statement = session.prepareStatement(REMOVE_SUBSCRIPTION);
         statement.setString(1, ds.getClientID());
         statement.setString(2, ds.getName());
         if (statement.executeUpdate() != 1)
            throw new JMSException("Durable subscription does not exist " + ds);
      }
      catch (SQLException e)
      {
         session.setRollbackOnly();
         throw new SpyJMSException("Error removing durable subscription " + ds, e);
      }
      finally
      {
         session.close();
      }      
   }

   public Collection getDurableSubscriptionIdsForTopic(SpyTopic topic)
      throws JMSException
   {
      ArrayList result = new ArrayList();

      JDBCSession session = new JDBCSession();
      try
      {
         PreparedStatement statement = session.prepareStatement(GET_SUBSCRIPTIONS_FOR_TOPIC);
         statement.setString(1, topic.getName());
         ResultSet rs = statement.executeQuery();
         while (rs.next())
         {
            result.add(new DurableSubscriptionID(rs.getString(1), rs.getString(2), rs.getString(3)));
         }

         return result;
      }
      catch (SQLException e)
      {
         session.setRollbackOnly();
         throw new SpyJMSException("Error getting durable subscriptions for topic " + topic, e);
      }
      finally
      {
         session.close();
      }      
   }

   protected void checkLoggedOnClientId(String clientID)
      throws JMSException
   {
      JDBCSession session = new JDBCSession();
      try
      {
         PreparedStatement statement = session.prepareStatement(GET_USER_BY_CLIENTID);
         statement.setString(1, clientID);
         ResultSet rs = statement.executeQuery();
         if (rs.next())
            throw new InvalidClientIDException("This client id is password protected " + clientID);
      }
      catch (SQLException e)
      {
         session.setRollbackOnly();
         throw new SpyJMSException("Error checking logged on client id " + clientID, e);
      }
      finally
      {
         session.close();
      }      
   }

   protected String getPreconfClientId(String logon, String passwd)
      throws JMSException
   {
      JDBCSession session = new JDBCSession();
      try
      {
         PreparedStatement statement = session.prepareStatement(GET_USER);
         statement.setString(1, logon);
         ResultSet rs = statement.executeQuery();
         if (rs.next() == false)
         {
            if (hasSecurityManager)
               return null;
            else
               throw new JMSSecurityException("This user does not exist " + logon);
         }

         if (hasSecurityManager == false && passwd.equals(rs.getString(1)) == false)
            throw new JMSSecurityException("Bad password for user " + logon);

         return rs.getString(2);
      }
      catch (SQLException e)
      {
         session.setRollbackOnly();
         throw new SpyJMSException("Error retrieving preconfigured user " + logon, e);
      }
      finally
      {
         session.close();
      }      
   }

   public StateManager getInstance()
   {
      return this;
   }

   // ServiceMBeanSupport overrides ---------------------------------

   protected void startService()
      throws Exception
   {
      if (connectionManagerName == null)
         throw new IllegalStateException("No connection manager configured");

      //Find the ConnectionFactoryLoader MBean so we can find the datasource
      String dsName = (String) getServer().getAttribute(connectionManagerName, "JndiName");

      InitialContext ctx = new InitialContext();
      try
      {
         dataSource = (DataSource) ctx.lookup("java:/" + dsName);
         tm = (TransactionManager) ctx.lookup(TransactionManagerService.JNDI_NAME);
      }
      finally
      {
         ctx.close();
      }

      initDB();
   }

   // Protected -----------------------------------------------------

   protected void initDB()
      throws Exception
   {
      CREATE_USER_TABLE = sqlProperties.getProperty("CREATE_USER_TABLE", CREATE_USER_TABLE);
      CREATE_ROLE_TABLE = sqlProperties.getProperty("CREATE_ROLE_TABLE", CREATE_ROLE_TABLE);
      CREATE_SUBSCRIPTION_TABLE = sqlProperties.getProperty("CREATE_SUBSCRIPTION_TABLE", CREATE_SUBSCRIPTION_TABLE);
      GET_SUBSCRIPTION = sqlProperties.getProperty("GET_SUBSCRIPTION", GET_SUBSCRIPTION);
      GET_SUBSCRIPTIONS_FOR_TOPIC = sqlProperties.getProperty("GET_SUBSCRIPTIONS_FOR_TOPIC", GET_SUBSCRIPTIONS_FOR_TOPIC);
      LOCK_SUBSCRIPTION = sqlProperties.getProperty("LOCK_SUBSCRIPTION", LOCK_SUBSCRIPTION);
      INSERT_SUBSCRIPTION = sqlProperties.getProperty("INSERT_SUBSCRIPTION", INSERT_SUBSCRIPTION);
      UPDATE_SUBSCRIPTION = sqlProperties.getProperty("UPDATE_SUBSCRIPTION", UPDATE_SUBSCRIPTION);
      REMOVE_SUBSCRIPTION = sqlProperties.getProperty("REMOVE_SUBSCRIPTION", REMOVE_SUBSCRIPTION);
      GET_USER_BY_CLIENTID = sqlProperties.getProperty("GET_USER_BY_CLIENTID", GET_USER_BY_CLIENTID);
      GET_USER = sqlProperties.getProperty("GET_USER", GET_USER);
      createTables = sqlProperties.getProperty("CREATE_TABLES_ON_START_UP", "true").equalsIgnoreCase("true");

      if (createTables)
      {
         JDBCSession session = new JDBCSession();
         try
         {
            PreparedStatement statement;
            try
            {
               statement = session.prepareStatement(CREATE_USER_TABLE);
               statement.executeUpdate();
            }
            catch (SQLException ignored)
            {
               log.debug("Error creating table: " + CREATE_USER_TABLE, ignored);
            }
            try
            {
               statement = session.prepareStatement(CREATE_ROLE_TABLE);
               statement.executeUpdate();
            }
            catch (SQLException ignored)
            {
               log.debug("Error creating table: " + CREATE_ROLE_TABLE, ignored);
            }
            try
            {
               statement = session.prepareStatement(CREATE_SUBSCRIPTION_TABLE);
               statement.executeUpdate();
            }
            catch (SQLException ignored)
            {
               log.debug("Error creating table: " + CREATE_SUBSCRIPTION_TABLE, ignored);
            }
         }
         finally
         {
            session.close();
         }      
      }
   }

   // Package Private -----------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

   /**
    * This inner class helps handle the jdbc connections.
    */
   class JDBCSession
   {
      boolean trace = log.isTraceEnabled();
      Transaction threadTx;
      Connection connection;
      HashSet statements = new HashSet();

      JDBCSession()
         throws JMSException
      {
         try
         {
            threadTx = tm.suspend();

            // Always begin a transaction
            tm.begin();

            // Retrieve a connection
            connection = dataSource.getConnection();
         }
         catch (Exception e)
         {
            try
            {
               if (connection != null)
                  connection.close();
            }
            catch (Throwable ignored)
            {
               if (trace)
                  log.trace("Unable to close connection", ignored);
            }
            try
            {
               if (threadTx != null)
                  tm.resume(threadTx);
            }
            catch (Throwable ignored)
            {
               if (trace)
                  log.trace("Unable to resume transaction " + threadTx, ignored);
            }
            throw new SpyJMSException("Error creating connection to the database.", e);
         }
      }

      PreparedStatement prepareStatement(String sql)
         throws SQLException
      {
         PreparedStatement result = connection.prepareStatement(sql);
         statements.add(result);
         return result;
      }

      void setRollbackOnly()
          throws JMSException
      {
         try
         {
            tm.setRollbackOnly();
         }
         catch (Exception e)
         {
            throw new SpyJMSException("Could not mark the transaction for rollback.", e);
         }
      }

      void close()
         throws JMSException
      {
         for (Iterator i = statements.iterator(); i.hasNext();)
         {
             Statement s = (Statement) i.next();
             try
             {
                s.close();
             }
             catch (Throwable ignored)
             {
               if (trace)
                  log.trace("Unable to close statement", ignored);
             }
         }

         try
         {
            if (connection != null)
               connection.close();
         }
         catch (Throwable ignored)
         {
            if (trace)
               log.trace("Unable to close connection", ignored);
         }

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
            throw new SpyJMSException("Could not commit/rollback a transaction with the transaction manager.", e);
         }
         finally
         {
            try
            {
               if (threadTx != null)
                  tm.resume(threadTx);
            }
            catch (Throwable ignored)
            {
               if (trace)
                  log.trace("Unable to resume transaction " + threadTx, ignored);
            }
         }
      }
   }
}

