
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.resource.adapter.jdbc;


import java.io.PrintWriter;
import java.io.Serializable;
import java.sql.Connection;
import java.sql.SQLException;
import java.util.Iterator;
import java.util.Properties;
import java.util.Set;
import java.util.HashMap;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.security.PasswordCredential;
import javax.security.auth.Subject;
import org.jboss.logging.Logger;
import org.jboss.resource.JBossResourceException;

/**
 * BaseWrapperManagedConnectionFactory.java
 *
 *
 * Created: Fri Apr 19 13:33:08 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public abstract class BaseWrapperManagedConnectionFactory
   implements ManagedConnectionFactory, Serializable
{
   protected final Logger log = Logger.getLogger(getClass());

   protected String userName;
   protected String password;

   //This is used by Local wrapper for all properties, and is left
   //in this class for ease of writing getConnectionProperties,
   //which always holds the user/pw.
   protected HashMap connectionProps = new HashMap();

   protected int transactionIsolation = -1;

   protected int preparedStatementCacheSize = 0;
   protected boolean doQueryTimeout = false;
   /**
    * The variable <code>newConnectionSQL</code> holds an SQL
    * statement which if not null is executed when a new Connection is
    * obtained for a new ManagedConnection.
    *
    */
   protected String newConnectionSQL;

   /**
    * The variable <code>checkValidConnectionSQL</code> holds an sql
    * statement that may be executed whenever a managed connection is
    * removed from the pool, to check that it is still valid.  This
    * requires setting up an mbean to execute it when notified by the
    * ConnectionManager.
    *
    */
   protected String checkValidConnectionSQL;

   /**
    * The classname used to check whether a connection is valid
    */
   protected String validConnectionCheckerClassName;

   /**
    * The instance of the valid connection checker
    */
   protected ValidConnectionChecker connectionChecker;

   private String exceptionSorterClassName;

   private ExceptionSorter exceptionSorter;

   private boolean trackStatements;

   public BaseWrapperManagedConnectionFactory ()
   {

   }
   // implementation of javax.resource.spi.ManagedConnectionFactory interface

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public PrintWriter getLogWriter() throws ResourceException
   {
      // TODO: implement this javax.resource.spi.ManagedConnectionFactory method
      return null;
   }

   /**
    *
    * @param param1 <description>
    * @exception javax.resource.ResourceException <description>
    */
   public void setLogWriter(PrintWriter param1) throws ResourceException
   {
      // TODO: implement this javax.resource.spi.ManagedConnectionFactory method
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Object createConnectionFactory(ConnectionManager cm) throws ResourceException
   {
      return new WrapperDataSource(this, cm);
   }

   /**
    *
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public Object createConnectionFactory() throws ResourceException
   {
      throw new JBossResourceException("NYI");
      //return createConnectionFactory(new DefaultConnectionManager());
   }

   //-----------Property setting code
   /**
    * Get the value of userName.
    * @return value of userName.
    */
   public String getUserName() {
      return userName;
   }

   /**
    * Set the value of userName.
    * @param v  Value to assign to userName.
    */
   public void setUserName(final String  userName) {
      this.userName = userName;
   }

   /**
    * Get the value of password.
    * @return value of password.
    */
   public String getPassword() {
      return password;
   }

   /**
    * Set the value of password.
    * @param v  Value to assign to password.
    */
   public void setPassword(final String  password) {
      this.password = password;
   }

   public int getPreparedStatementCacheSize()
   {
      return preparedStatementCacheSize;
   }
   public void setPreparedStatementCacheSize(int size)
   {
      preparedStatementCacheSize = size;
   }
   public boolean getTxQueryTimeout()
   {
      return doQueryTimeout;
   }
   public void setTxQueryTimeout(boolean qt)
   {
      doQueryTimeout = qt;
   }
   /**
    * Gets the TransactionIsolation attribute of the JDBCManagedConnectionFactory
    * object
    *
    * @return   The TransactionIsolation value
    */
   public String getTransactionIsolation()
   {
      switch (this.transactionIsolation)
      {
         case Connection.TRANSACTION_NONE:
            return "TRANSACTION_NONE";
         case Connection.TRANSACTION_READ_COMMITTED:
            return "TRANSACTION_READ_COMMITTED";
         case Connection.TRANSACTION_READ_UNCOMMITTED:
            return "TRANSACTION_READ_UNCOMMITTED";
         case Connection.TRANSACTION_REPEATABLE_READ:
            return "TRANSACTION_REPEATABLE_READ";
         case Connection.TRANSACTION_SERIALIZABLE:
            return "TRANSACTION_SERIALIZABLE";
         case -1:
            return "DEFAULT";
         default:
            return Integer.toString(transactionIsolation);
      }
   }


   /**
    * Sets the TransactionIsolation attribute of the JDBCManagedConnectionFactory
    * object
    *
    * @param transactionIsolation  The new TransactionIsolation value
    */
   public void setTransactionIsolation(String transactionIsolation)
   {
      if (transactionIsolation.equals("TRANSACTION_NONE"))
      {
         this.transactionIsolation = Connection.TRANSACTION_NONE;
      }
      else if (transactionIsolation.equals("TRANSACTION_READ_COMMITTED"))
      {
         this.transactionIsolation = Connection.TRANSACTION_READ_COMMITTED;
      }
      else if (transactionIsolation.equals("TRANSACTION_READ_UNCOMMITTED"))
      {
         this.transactionIsolation = Connection.TRANSACTION_READ_UNCOMMITTED;
      }
      else if (transactionIsolation.equals("TRANSACTION_REPEATABLE_READ"))
      {
         this.transactionIsolation = Connection.TRANSACTION_REPEATABLE_READ;
      }
      else if (transactionIsolation.equals("TRANSACTION_SERIALIZABLE"))
      {
         this.transactionIsolation = Connection.TRANSACTION_SERIALIZABLE;
      }
      else
      {
         try
         {
            this.transactionIsolation = Integer.parseInt(transactionIsolation);
         }
         catch (NumberFormatException nfe)
         {
            throw new IllegalArgumentException("Setting Isolation level to unknown state: " + transactionIsolation);
         }
      }
   }


   /**
    * Get the NewConnectionSQL value.
    * @return the NewConnectionSQL value.
    */
   public String getNewConnectionSQL()
   {
      return newConnectionSQL;
   }

   /**
    * Set the NewConnectionSQL value.
    * @param newNewConnectionSQL The new NewConnectionSQL value.
    */
   public void setNewConnectionSQL(String newConnectionSQL)
   {
      this.newConnectionSQL = newConnectionSQL;
   }


   /**
    * Get the CheckValidConnectionSQL value.
    * @return the CheckValidConnectionSQL value.
    */
   public String getCheckValidConnectionSQL()
   {
      return checkValidConnectionSQL;
   }

   /**
    * Set the CheckValidConnectionSQL value.
    * @param newCheckValidConnectionSQL The new CheckValidConnectionSQL value.
    */
   public void setCheckValidConnectionSQL(String checkValidConnectionSQL)
   {
      this.checkValidConnectionSQL = checkValidConnectionSQL;
   }


   /**
    * Whether to track statements
    * @return true when tracking statements
    */
   public boolean getTrackStatements()
   {
      return trackStatements;
   }

   /**
    * Set the track statements value.
    * @param value true to track statements.
    */
   public void setTrackStatements(boolean value)
   {
      trackStatements = value;
   }

   /**
    * Get the ExceptionSorterClassname value.
    * @return the ExceptionSorterClassname value.
    */
   public String getExceptionSorterClassName()
   {
      return exceptionSorterClassName;
   }

   /**
    * Set the ExceptionSorterClassname value.
    * @param exceptionSorterClassName The new ExceptionSorterClassName value.
    */
   public void setExceptionSorterClassName(String exceptionSorterClassName)
   {
      this.exceptionSorterClassName = exceptionSorterClassName;
   }

   /**
    * Get the valid connection checker class name
    *
    * @return the class name
    */
   public String getValidConnectionCheckerClassName()
   {
      return validConnectionCheckerClassName;
   }

   /**
    * Set the valid connection checker class name
    *
    * @param className the class name
    */
   public void setValidConnectionCheckerClassName(String value)
   {
      validConnectionCheckerClassName = value;
   }
 
   /**
    * Gets full set of connection properties, i.e. whatever is provided
    * in config plus "user" and "password" from subject/cri.
    *
    * <p>Note that the set is used to match connections to datasources as well
    * as to create new managed connections.
    *
    * <p>In fact, we have a problem here. Theoretically, there is a possible
    * name collision between config properties and "user"/"password".
    */
   protected Properties getConnectionProperties(Subject subject, ConnectionRequestInfo cri)
      throws ResourceException
   {
      if (cri != null && cri.getClass() != WrappedConnectionRequestInfo.class)
      {
         throw new JBossResourceException("Wrong kind of ConnectionRequestInfo: " + cri.getClass());
      } // end of if ()

      // NOTE: we do this because we do not have to synchronize at all on the connectionProps
      // AVOID GLOBAL Synchronizations.  (Bill Burke)
      Properties props = new Properties();
      props.putAll(connectionProps);
      if (subject != null)
      {
         for (Iterator i = subject.getPrivateCredentials().iterator(); i.hasNext(); )
         {
            Object o = i.next();
            if (o instanceof PasswordCredential && ((PasswordCredential)o).getManagedConnectionFactory().equals(this))
            {
               PasswordCredential cred = (PasswordCredential)o;
               props.setProperty("user", (cred.getUserName() == null)? "": cred.getUserName());
               props.setProperty("password", new String(cred.getPassword()));
               return props;
            } // end of if ()
         } // end of for ()
         throw new JBossResourceException("No matching credentials in Subject!");
      } // end of if ()
      WrappedConnectionRequestInfo lcri = (WrappedConnectionRequestInfo)cri;
      if (lcri != null)
      {
         props.setProperty("user", (lcri.getUserName() == null)? "": lcri.getUserName());
         props.setProperty("password", (lcri.getPassword() == null)? "": lcri.getPassword());
         return props;
      } // end of if ()
      if (userName != null)
      {
         props.setProperty("user", userName);
         props.setProperty("password", (password == null) ? "" : password);
      }
      return props;
   }

   boolean isExceptionFatal(SQLException e)
   {
      if (exceptionSorter != null)
         return exceptionSorter.isExceptionFatal(e);

      if (exceptionSorterClassName != null)
      {
         try
         {
            ClassLoader cl = Thread.currentThread().getContextClassLoader();
            Class clazz = cl.loadClass(exceptionSorterClassName);
            exceptionSorter = (ExceptionSorter)clazz.newInstance();
            return exceptionSorter.isExceptionFatal(e);
         }
         catch (Exception e2)
         {
            log.warn("exception trying to create exception sorter (disabling):", e2);
            exceptionSorter = new NullExceptionSorter();
         }
      }
      return false;
   }

   /**
    * Checks whether a connection is valid
    */
   SQLException isValidConnection(Connection c)
   {
      // Already got a checker
      if (connectionChecker != null)
         return connectionChecker.isValidConnection(c);

      // Class specified
      if (validConnectionCheckerClassName != null)
      {
         try
         {
            ClassLoader cl = Thread.currentThread().getContextClassLoader();
            Class clazz = cl.loadClass(validConnectionCheckerClassName);
            connectionChecker = (ValidConnectionChecker) clazz.newInstance();
            return connectionChecker.isValidConnection(c);
         }
         catch (Exception e)
         {
            log.warn("Exception trying to create connection checker (disabling):", e);
            connectionChecker = new NullValidConnectionChecker();
         }
      }

      // SQL statement specified
      if (checkValidConnectionSQL != null)
      {
         connectionChecker = new CheckValidConnectionSQL(checkValidConnectionSQL);
         return connectionChecker.isValidConnection(c);
      }

      // No Check
      return null;
   }

}// BaseWrapperManagedConnectionFactory
