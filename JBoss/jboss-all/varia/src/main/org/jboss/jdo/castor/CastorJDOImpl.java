/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jdo.castor;

import java.io.PrintWriter;
import java.io.Serializable;
import java.io.FileNotFoundException;

import java.lang.reflect.Method;

import java.util.HashMap;
import java.util.Hashtable;

import java.util.Enumeration;
import java.net.URL;

import javax.management.*;
import javax.naming.spi.ObjectFactory;
import javax.naming.Referenceable;
import javax.naming.Reference;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.NameNotFoundException;

import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;

import org.exolab.castor.jdo.Database;
import org.exolab.castor.jdo.DataObjects;
import org.exolab.castor.jdo.JDO;
import org.exolab.castor.jdo.DatabaseNotFoundException;
import org.exolab.castor.jdo.PersistenceException;
import org.exolab.castor.persist.spi.LogInterceptor;
import org.exolab.castor.xml.Unmarshaller;

import org.jboss.logging.util.LoggerWriter;

import org.jboss.system.ServiceMBeanSupport;

import org.jboss.proxy.compiler.Proxy;
import org.jboss.proxy.compiler.Proxies;
import org.jboss.proxy.compiler.InvocationHandler;

/**
 * Castor JDO support.
 *
 * @jmx:mbean name="jboss:type=Service,service=JDO,flavor=Castor"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @version <tt>$Revision: 1.14 $</tt>
 * @author Oleg Nitz (on@ibis.odessa.ua)
 */
public class CastorJDOImpl
   extends ServiceMBeanSupport
   implements DataObjects, ObjectFactory, Referenceable, Serializable,
              CastorJDOImplMBean, MBeanRegistration, LogInterceptor
{

   private String _jndiName;

   private String _dbConf;

   private String _dbUrl;

   private JDO _jdo = new JDO();

   private String _dataSourceName;

   private static HashMap _instances = new HashMap();

   private transient PrintWriter writer;

   /**
    * Do JDO classes should be loader by the global class loader
    * (the same class loader as Castor classes)?
    */
   private boolean _commonClassPath;

   /*
    * True if user prefer all reachable object to be stored automatically.
    * False (default) if user want only dependent object to be stored.
    */
   private boolean _autoStore = false;

   /*
    * True if user prefers application-server database pooling.
    * False (default) if user wants a new connection for each invocation of
    * getDatabase().
    */
   private boolean _dbPooling = false;

   public CastorJDOImpl() {
   }

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
      throws javax.management.MalformedObjectNameException
   {
      if (name == null) {
         return new ObjectName(OBJECT_NAME+",name="+_jndiName);
      }
      
      return name;
   }

   protected void startService() throws Exception
   {
      org.exolab.castor.jdo.conf.Database database;
      Unmarshaller unm;
      int pos;
      Method m;

      boolean debug = log.isDebugEnabled();

      // Bind in JNDI
      bind(new InitialContext(), "java:/" + _jndiName, this);

		// Determine complete URL to _dbConf file.  It is likely a relative path.
		URL		confUrl = Thread.currentThread().getContextClassLoader().getResource( _dbConf );

		if ( null == confUrl )
			{
			FileNotFoundException e = new FileNotFoundException(
				"CastorJDOImpl.startService(): Unable to resolve Configuration attribute to URL = "
				+ _dbConf );
			log.error( "CastorJDOImpl.startService(): Unable to find " + _dbConf + " file.", e );
			}

		_dbUrl = confUrl.toString();
	
      _jdo.setTransactionManager("java:/TransactionManager");
      _jdo.setConfiguration( _dbUrl );
      unm = new Unmarshaller(org.exolab.castor.jdo.conf.Database.class);
      database = (org.exolab.castor.jdo.conf.Database) unm.unmarshal(new InputSource( _dbUrl ));
      _jdo.setDatabaseName(database.getName());
      if (database.getJndi() != null) {
         _dataSourceName = database.getJndi().getName();
      }
      // Older Castor versions older don't have these methods,
      // we'll use reflection for backward compatibility
      //_jdo.setAutoStore(_autoStore);
      //_jdo.setDatabasePooling(_dbpooling);
      try {
         // 0.9.4
         m = _jdo.getClass().getMethod("setAutoStore",
                                       new Class[] {boolean.class});
         m.invoke(_jdo, new Object[] {new Boolean(_autoStore)});
      } catch (Exception ex) {
         if (debug)
            log.debug("couldn't invoke setAutoStore()");
      }

      try {
         // 0.9.3
         m = _jdo.getClass().getMethod("setDatabasePooling",
                                       new Class[] {boolean.class});
         m.invoke(_jdo, new Object[] {new Boolean(_dbPooling)});
      } catch (Exception ex) {
         if (debug)
            log.debug("couldn't invoke setDatabasePooling()");
      }
      _instances.put(_jndiName, this);
      if (debug)
         log.debug("DataObjects factory for " + _dataSourceName + " bound to " + _jndiName);
   }

   protected void stopService() throws Exception
   {
      // Unbind from JNDI
      InitialContext ctx = new InitialContext();
      try {
         ctx.unbind("java:/" + _jndiName);
      }
      finally {
         ctx.close();
      }
   }


	/**
		* Copied from Jetty.java.  Used to find resource within .sar.
	*/
	public URL findResourceInJar( String name )
	{
		URL url = null;

		try
			{
			url = getClass().getClassLoader().getResource( name );
			}
		catch ( Exception e )
			{
			log.error( "Could not find resource: " + name, e );
			}

		return url;
	}


   // CastorJDOImplMBean implementation ---------------------------

   /**
    * @jmx:managed-attribute
    */
   public void setJndiName(String jndiName) {
      _jndiName = jndiName;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJndiName() {
      return _jndiName;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setConfiguration(String dbConf) {
      _dbConf = dbConf;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getConfiguration() {
      return _dbConf;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getConfigurationURL() {
      return _dbUrl;
   }

   /**
    * @jmx:managed-attribute
    */
   public void setLockTimeout(int lockTimeout) {
      _jdo.setLockTimeout(lockTimeout);
   }

   /**
    * @jmx:managed-attribute
    */
   public int getLockTimeout() {
      return _jdo.getLockTimeout();
   }

   /**
    * @jmx:managed-attribute
    */
   public void setLoggingEnabled(boolean loggingEnabled) {
      _jdo.setLogInterceptor(loggingEnabled ? this : null);
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean getLoggingEnabled() {
      return (_jdo.getLogInterceptor() != null);
   }

   /**
    * @jmx:managed-attribute
    */
   public void setCommonClassPath(boolean commonClassPath) {
      _commonClassPath = commonClassPath;
   }

   /**
    * @jmx:managed-attribute
    */
   public boolean getCommonClassPath() {
      return _commonClassPath;
   }

   /**
    * @jmx:managed-attribute
    *
    * @param autoStore    True if user prefer all reachable object to be stored automatically.
    *                     False if user want only dependent object to be stored.
    */
   public void setAutoStore( boolean autoStore ) {
      _autoStore = autoStore;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return    if the next Database instance will be set to autoStore.
    */
   public boolean isAutoStore() {
      return _autoStore;
   }

   /**
    * True if user prefers to use application server database pools.
    * False if user wants a new connection for each call to getDatabase().
    *
    * @jmx:managed-attribute
    */
   public void setDatabasePooling(boolean dbPooling) {
      _dbPooling = dbPooling;
   }

   /**
    * Return true if the Database instance uses the application server pooling.
    *
    * @jmx:managed-attribute
    */
   public boolean isDatabasePooling() {
      return _dbPooling;
   }

   
   // DataObjects implementation ----------------------------------

   public Database getDatabase()
      throws DatabaseNotFoundException, PersistenceException
   {
      Method m;

      if (_commonClassPath) {
         _jdo.setClassLoader(null);
      } else {
         _jdo.setClassLoader(Thread.currentThread().getContextClassLoader());
      }
      return _jdo.getDatabase();
   }

   public void setDescription(String description) {
      _jdo.setDescription(description);
   }

   public String getDescription() {
      return _jdo.getDescription();
   }

   
   // Referenceable implementation ----------------------------------
   
   public Reference getReference() {
      return new Reference(getClass().getName(), getClass().getName(), null);
   }

   
   // ObjectFactory implementation ----------------------------------
   
   public Object getObjectInstance(Object obj,
                                   Name name,
                                   Context nameCtx,
                                   Hashtable environment)
      throws Exception
   {
      return _instances.get(name.toString());
   }

   
   // Private -------------------------------------------------------
   
   private void bind(Context ctx, String name, Object val)
      throws NamingException
   {
      // Bind val to name in ctx, and make sure that all intermediate contexts exist

      Name n = ctx.getNameParser("").parse(name);
      while (n.size() > 1)
         {
            String ctxName = n.get(0);
            try
               {
                  ctx = (Context)ctx.lookup(ctxName);
               } catch (NameNotFoundException e)
                  {
                     ctx = ctx.createSubcontext(ctxName);
                  }
            n = n.getSuffix(1);
         }

      ctx.bind(n.get(0), val);
   }

   
   // LogInterceptor implementation for Castor 0.8 ----------------------
   
   public void loading(Class objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Loading " + objClass.getName() + " (" + identity + ")" );
   }

   public void creating(Class objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Creating " + objClass.getName() + " (" + identity + ")" );
   }

   public void removing(Class objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Removing " + objClass.getName() + " (" + identity + ")" );
   }


   public void storing(Class objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Storing " + objClass.getName() + " (" + identity + ")" );
   }


   // LogInterceptor implementation for Castor 0.9 ----------------------
   
   public void loading(Object objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Loading " + objClass + " (" + identity + ")" );
   }

   public void creating(Object objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Creating " + objClass + " (" + identity + ")" );
   }

   public void removing(Object objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Removing " + objClass + " (" + identity + ")" );
   }

   public void storing(Object objClass, Object identity) {
      if (log.isDebugEnabled())
         log.debug( "Storing " + objClass + " (" + identity + ")" );
   }

   // LogInterceptor implementation - the rest part --------------------

   public void storeStatement(String statement) {
      log.debug(statement);
   }

   public void queryStatement(String statement) {
      log.debug(statement);
   }

   public void message(String message) {
      log.debug(message);
   }

   public void exception(Exception except) {
      log.error("Exception", except);
   }

   public PrintWriter getPrintWriter()
   {
      if (writer == null)
      {
         writer = new LoggerWriter(((org.jboss.logging.Log4jLoggerPlugin)log.getLoggerPlugin ()).getLogger ());
      }
      
      return writer;
   }
}

