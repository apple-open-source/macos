/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jca.jdbc;

import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Statement;
import java.sql.PreparedStatement;
import java.sql.CallableStatement;
import java.sql.DatabaseMetaData;
import java.util.Map;
import java.sql.SQLWarning;
import java.sql.Savepoint;


/**
 * TestConnection.java
 *
 *
 * Created: Fri Feb 14 13:19:39 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestConnection implements Connection {

   private TestDriver driver;

   private boolean autocommit;

   private boolean closed;

   public TestConnection(TestDriver driver)
   {
      this.driver = driver;
   }

   public void setFail(boolean fail)
   {
      driver.setFail(fail);
   }

   public int getClosedCount()
   {
      return driver.getClosedCount();
   }

   // Implementation of java.sql.Connection

   public Statement createStatement(int n, int n1, int n2) throws SQLException {
      return null;
   }

   public void clearWarnings()
   {
   }

   public void close()
   {
      closed = true;
      driver.connectionClosed();
   }

   public void commit()
   {
   }

   public Statement createStatement() throws SQLException
   {
      return new TestStatement(driver);
   }

   public Statement createStatement(int rst, int rsc) throws SQLException
   {
      return null;
   }

   public boolean getAutoCommit()
   {
      return autocommit;
   }

   public void setAutoCommit(boolean autocommit)
   {
      this.autocommit = autocommit;
   }

   public String getCatalog()
   {
      return null;
   }

   public DatabaseMetaData getMetaData()
   {
      return null;
   }

   public int getTransactionIsolation()
   {
      return 0;
   }

   public Map getTypeMap()
   {
      return null;
   }

   public SQLWarning getWarnings()
   {
      return null;
   }

   public boolean isClosed()
   {
      return closed;
   }

   public boolean isReadOnly()
   {
      return false;
   }

   public String nativeSQL(String sql)
   {
      return sql;
   }

   public CallableStatement prepareCall(String sql)
   {
      return null;
   }

   public CallableStatement prepareCall(String sql, int rst)
   {
      return null;
   }

   public CallableStatement prepareCall(String sql, int[] rst)
   {
      return null;
   }

   public CallableStatement prepareCall(String sql, int rst, int rsc)
   {
      return null;
   }

   public CallableStatement prepareCall(String sql, String[] rst)
   {
      return null;
   }

   public CallableStatement prepareCall(String sql, int rst, int rsc, int i)
   {
      return null;
   }

   public PreparedStatement prepareStatement(String sql)
   {
      return null;
   }

   public PreparedStatement prepareStatement(String sql, int rst, int rsc)
   {
      return null;
   }

   public PreparedStatement prepareStatement(String sql, int rst)
   {
      return null;
   }

   public PreparedStatement prepareStatement(String sql, int[] rst)
   {
      return null;
   }

   public PreparedStatement prepareStatement(String sql, String[] rst)
   {
      return null;
   }

   public PreparedStatement prepareStatement(String sql, int rst, int rsc, int i)
   {
      return null;
   }

   public void rollback()
   {
   }

   public void setCatalog(String cat)
   {
   }

   public void setReadOnly(boolean r0)
   {
   }

   public void setTransactionIsolation(int level)
   {
   }

   public void setTypeMap(Map map)
   {
   }

   public void setHoldability(int h)
   {
   }

   public int getHoldability()
   {
      return 0;
   }

   public Savepoint setSavepoint()
   {
      return null;
   }

   public Savepoint setSavepoint(String name)
   {
      return null;
   }

   public void rollback(Savepoint s)
   {
   }
   public void commit(Savepoint s)
   {
   }

   public void releaseSavepoint(Savepoint s)
   {
   }

}// TestConnection
