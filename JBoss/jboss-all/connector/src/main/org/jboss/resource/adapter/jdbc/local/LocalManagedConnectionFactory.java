/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.resource.adapter.jdbc.local;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.sql.Connection;
import java.sql.Driver;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.util.Iterator;
import java.util.Properties;
import java.util.Set;
import java.util.HashMap;
import javax.resource.ResourceException;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

import org.jboss.util.NestedRuntimeException;
import org.jboss.resource.JBossResourceException;
import org.jboss.resource.adapter.jdbc.BaseWrapperManagedConnectionFactory;

/**
 * LocalManagedConnectionFactory
 *
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class LocalManagedConnectionFactory extends BaseWrapperManagedConnectionFactory
{
   private String driverClass;
   private transient Driver driver;
   private String connectionURL;
   protected String connectionProperties;

   public LocalManagedConnectionFactory()
   {

   }

   //-----------Property setting code
   /**
    * Get the value of ConnectionURL.
    * @return value of ConnectionURL.
    */
   public String getConnectionURL() {
      return connectionURL;
   }

   /**
    * Set the value of ConnectionURL.
    * @param v  Value to assign to ConnectionURL.
    */
   public void setConnectionURL(final String  connectionURL) {
      this.connectionURL = connectionURL;
   }

   /**
    * Get the DriverClass value.
    * @return the DriverClass value.
    */
   public String getDriverClass()
   {
      return driverClass;
   }

   /**
    * Set the DriverClass value.
    * @param newDriverClass The new DriverClass value.
    */
   public synchronized void setDriverClass(final String driverClass)
   {
      this.driverClass = driverClass;
      driver = null;
   }

   /**
    * Get the value of connectionProperties.
    * @return value of connectionProperties.
    */
   public String getConnectionProperties() {
      return connectionProperties;
   }

   /**
    * Set the value of connectionProperties.
    * @param v  Value to assign to connectionProperties.
    */
   public void setConnectionProperties(String  connectionProperties)
   {
      this.connectionProperties = connectionProperties;
      if (connectionProperties != null)
      {
         Properties loadit = new Properties();
         InputStream is =
            new ByteArrayInputStream(connectionProperties.getBytes());
         try
         {
            loadit.load(is);
         }
         catch (IOException ioe)
         {
            throw new NestedRuntimeException(
               "Could not load connection properties",
               ioe);
         }
         HashMap tmp = new HashMap();
         tmp.putAll(loadit);
         connectionProps = tmp;
      }
      else
         connectionProps = new HashMap();

   }

   //Real ManagedConnectionFactory methods
   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public ManagedConnection createManagedConnection(Subject subject, ConnectionRequestInfo cri)
      throws javax.resource.ResourceException
   {
      Properties props = getConnectionProperties(subject, cri);
      // Some friendly drivers (Oracle, you guessed right) modify the props you supply.
      // Since we use our copy to identify compatibility in matchManagedConnection, we need
      // a pristine copy for our own use.  So give the friendly driver a copy.
      Properties copy = (Properties) props.clone();
      if (log.isDebugEnabled()) {
         log.debug("Using properties: "  + props);
      }

      try
      {
         String url = getConnectionURL();
         Driver d = getDriver(url);
         Connection con = d.connect(url, copy);
         if (con == null)
         {
            throw new JBossResourceException("Wrong driver class for this connection URL");
         } // end of if ()

         return new LocalManagedConnection(this, con, props, transactionIsolation, preparedStatementCacheSize, doQueryTimeout);
      }
      catch (Exception e)
      {
         throw new JBossResourceException("Could not create connection", e);
      } // end of try-catch
   }

   /**
    *
    * @param param1 <description>
    * @param param2 <description>
    * @param param3 <description>
    * @return <description>
    * @exception javax.resource.ResourceException <description>
    */
   public ManagedConnection matchManagedConnections(Set mcs, Subject subject, ConnectionRequestInfo cri)
      throws ResourceException
   {
      Properties newProps = getConnectionProperties(subject, cri);
      for (Iterator i = mcs.iterator(); i.hasNext(); )
      {
         Object o = i.next();
         if (o instanceof LocalManagedConnection)
         {
            LocalManagedConnection mc = (LocalManagedConnection)o;
            if (mc.getProps().equals(newProps) && mc.checkValid())
            {
               return mc;
            } // end of if ()

         } // end of if ()
      } // end of for ()
      return null;
   }

   /**
    *
    * @return hashcode computed according to recommendations in Effective Java.
    */
   public int hashCode()
   {
      int result = 17;
      result = result * 37 + ((connectionURL == null)? 0: connectionURL.hashCode());
      result = result * 37 + ((driverClass == null)? 0: driverClass.hashCode());
      result = result * 37 + ((userName == null)? 0: userName.hashCode());
      result = result * 37 + ((password == null)? 0: password.hashCode());
      result = result * 37 + transactionIsolation;
      return result;
   }

   /**
    *
    * @param param1 <description>
    * @return <description>
    */
   public boolean equals(Object other)
   {
      if (this == other)
      {
         return true;
      } // end of if ()
      if (getClass() != other.getClass())
      {
         return false;
      } // end of if ()
      LocalManagedConnectionFactory otherMcf = (LocalManagedConnectionFactory)other;
      return this.connectionURL.equals(otherMcf.connectionURL)
         && this.driverClass.equals(otherMcf.driverClass)
         && ((this.userName == null) ? otherMcf.userName == null:
             this.userName.equals(otherMcf.userName))
         && ((this.password == null) ? otherMcf.password == null:
             this.password.equals(otherMcf.password))
         && this.transactionIsolation == otherMcf.transactionIsolation;

   }

   //protected access

   /**
    * Check the driver for the given URL.  If it is not registered already
    * then register it.
    *
    * @param url   The JDBC URL which we need a driver for.
    */
   protected synchronized Driver getDriver(final String url) throws ResourceException
   {
      // don't bother if it is loaded already
      if (driver!=null)
      {
         return driver;
      }
      log.debug("Checking driver for URL: " + url);

      if (driverClass == null)
      {
         throw new JBossResourceException("No Driver class specified!");
      }

      // Check if the driver is already loaded, if not then try to load it

      if (isDriverLoadedForURL(url))
      {
         return driver;
      } // end of if ()

      try
      {
         //try to load the class... this should register with DriverManager.
         Class clazz = Class.forName(driverClass, true, Thread.currentThread().getContextClassLoader());
         if (isDriverLoadedForURL(url))
         {
            //return immediately, some drivers (Cloudscape) do not let you create an instance.
            return driver;
         } // end of if ()
         //We loaded the class, but either it didn't register
         //and is not spec compliant, or is the wrong class.
         driver = (Driver)clazz.newInstance();
         DriverManager.registerDriver(driver);
         if (isDriverLoadedForURL(url))
         {
            return driver;
         } // end of if ()
         //We can even instantiate one, it must be the wrong class for the URL.
      }
      catch (Exception e)
      {
         throw new JBossResourceException
            ("Failed to register driver for: " + driverClass, e);
      }

      throw new JBossResourceException("Apparently wrong driver class specified for URL: class: " + driverClass + ", url: " + url);
   }

   private boolean isDriverLoadedForURL(String url)
   {
      try
      {
         driver = DriverManager.getDriver(url);
         log.debug("Driver already registered for url: " + url);
         return true;
      }
      catch (Exception e)
      {
         log.debug("Driver not yet registered for url: " + url);
         return false;
      } // end of try-catch
   }

   protected String internalGetConnectionURL()
   {
      return connectionURL;
   }

}// LocalManagedConnectionFactory
