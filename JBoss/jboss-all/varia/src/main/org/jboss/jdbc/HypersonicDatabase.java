/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.jdbc;

import java.io.File;
import java.io.IOException;

import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.SQLException;
import java.sql.Statement;

import javax.management.*;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.server.ServerConfigLocator;

import org.jboss.logging.Logger;

import org.hsqldb.Server;
import org.hsqldb.util.DatabaseManagerSwing;

/**
 * Integration with <a href="http://sourceforge.net/projects/hsqldb">Hypersonic SQL</a> (c).
 * 
 * <p>Starts 1.7.1 Hypersonic database in-VM.
 * 
 * @jmx:mbean name="jboss:service=Hypersonic"
 *				  extends="org.jboss.system.ServiceMBean"
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:Scott_Stark@displayscape.com">Scott Stark</a>.
 * @author <a href="mailto:pf@iprobot.com">Peter Fagerlund</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @version $Revision: 1.17.2.1 $
 */
public class HypersonicDatabase 
	extends ServiceMBeanSupport 
	implements HypersonicDatabaseMBean, MBeanRegistration 
{
	/** HSQLDB server class. */
	org.hsqldb.Server embeddedDBServer;

	/** Full path to db/hypersonic. */
	File dbPath;

	/**
	 * Database name will be appended to <tt><em>jboss-server-data-dir</em></tt>.
	 */
	String name = "default";

	/** Default port. */
	int port = 1701;

	/** Default silent. */
	boolean silent = true;

	/** Default trace. */
	boolean trace = false;
	
	/** Default no_system_exit 
	*	 New embedded support in 1.7 
	*/
	boolean no_system_exit = true;
	
	/** Default persist 
	*	 Run with or without a hsqldb server instance
	*	 true == persistence over invocations
	*	 false == no persistence over invocations -- excelent for testing
	*/
	boolean persist = true;

	public HypersonicDatabase() {
		// empty
	}

	/**
	 * @jmx:managed-attribute
	 */
	public void setDatabase(final String name) {
		this.name = name;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public String getDatabase() {
		return name;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public void setPort(final int port) {
		this.port = port;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public int getPort() {
		return port;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public void setSilent(final boolean silent) {
		this.silent = silent;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public boolean getSilent() {
		return silent;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public void setTrace(final boolean trace) {
		this.trace = trace;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public boolean getTrace() {
		return trace;
	}
	
	/**
	 * @jmx:managed-attribute
	 */
	public void setNo_system_exit(final boolean no_system_exit) {
		this.no_system_exit = no_system_exit;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public boolean getNo_system_exit() {
		return no_system_exit;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public void setPersist(final boolean persist) {
		this.persist = persist;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public boolean getPersist() {
		return persist;
	}

	/**
	 * @jmx:managed-attribute
	 */
	public String getDatabasePath() {
		return dbPath.toString();
	}

	protected ObjectName getObjectName(MBeanServer server, ObjectName name)
		throws MalformedObjectNameException 
	{
		return name == null ? OBJECT_NAME : name;
	}

	/** 
	 * start of DatabaseManager accesible from the localhost:8082
	 *
	 * @jmx:managed-operation
	 */
	public void startDatabaseManager() {	
		// Start DBManager in new thread
		new Thread() {
	 public void run() {
		 try {
			String driver = "org.hsqldb.jdbcDriver";
			String[] args;
		  if(persist == true) {	 
				args = new String[] {
				"-driver", driver,
				"-url", "jdbc:hsqldb:hsql://localhost:" + port,
				"-user", "sa",
				"-password", "",
				"-dir", getDatabasePath(),
		  };
		  } else { 
				args = new String[] {
				"-driver", driver,
				"-url", "jdbc:hsqldb:hsql:.",
				"-user", "sa",
				"-password", "",
			};
		  }
		  org.hsqldb.util.DatabaseManagerSwing.main(args);
		 } 
		 catch (Exception e) { 
			 log.error("Failed to start database manager", e);
		 } 
		} 
		}.start();
	 }
	

	protected void startService() throws Exception {
		
		// Get the server data directory
		File dataDir = ServerConfigLocator.locate().getServerDataDir();
		
		// Get DB directory
		dbPath = new File(dataDir, "hypersonic");
		
		if (!dbPath.exists()) {
			dbPath.mkdirs();
		}
		
		if (!dbPath.isDirectory()) {
			throw new IOException("Failed to create directory: " + dbPath);
		}
		
		final File prefix = new File(dbPath, name);
		
		if(persist == true) {
		// Start DB in new thread, or else it will block us
		new Thread("hypersonic-" + name) {
	 public void run() {
		 try {
		  // Create startup arguments
		  String[] args = new String[] {
		  "-database",			prefix.toString(),
		  "-port",				String.valueOf(port), 
		  "-silent",			String.valueOf(silent), 
		  "-trace",				String.valueOf(trace),
		  "-no_system_exit", String.valueOf(no_system_exit),
			 };
			 
		  // Start server
		  embeddedDBServer.main(args);
		 } 
		 catch (Exception e) { 
			 log.error("Failed to start database", e);
		 }
	 }
		}.start();
	  }
	}

	/**
	 * We now close the connection clean by calling the
	 * serverSocket throught jdbc. The MBeanServer calls this 
	 * method at closing time ... this gives the db
	 * a chance to write out its memory cache ...
	 * or We issue a SHUTDOWN IMMEDIATELY if running as
	 * persist == false to not have hsqldb write out any file.
	 */
	protected void stopService() throws Exception {
	
		Connection connection;
		Statement statement;
		String jdbcDriver = "org.hsqldb.jdbcDriver";
		String dbURL = "jdbc:hsqldb:hsql://localhost:" + port;
		String noPersistdbURL = "jdbc:hsqldb:hsql:.";
		String user = "sa";
		String password = "";
					 
		if(persist == false) {
			connection = DriverManager.getConnection(noPersistdbURL, user, password);
			statement = connection.createStatement();
			statement.executeQuery("SHUTDOWN IMMEDIATELY");
		 } else {
		try {
			new org.hsqldb.jdbcDriver();
			Class.forName(jdbcDriver).newInstance();
			connection = DriverManager.getConnection(dbURL, user, password);
			statement = connection.createStatement();
			statement.executeQuery("SHUTDOWN COMPACT");
			log.info("Database closed clean");
		}
		finally {
			embeddedDBServer = null;
		}
		}
	}
}
