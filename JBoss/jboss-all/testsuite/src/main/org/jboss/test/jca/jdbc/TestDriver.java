/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jca.jdbc;

import java.sql.Driver;
import java.sql.SQLException;
import java.util.Properties;
import java.sql.Connection;
import java.sql.DriverPropertyInfo;

/**
 * TestDriver.java
 *
 *
 * Created: Fri Feb 14 12:15:42 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public class TestDriver implements Driver
{

   private boolean fail = false;

   private int closedCount = 0;


   public TestDriver() {

   }

   public void setFail(boolean fail)
   {
      this.fail = fail;
   }

   public boolean getFail()
   {
      return fail;
   }

   public int getClosedCount()
   {
      return closedCount;
   }

   public void connectionClosed()
   {
      closedCount++;
   }

   // Implementation of java.sql.Driver

   public boolean acceptsURL(String string) throws SQLException {
      return string != null && string.startsWith("jdbc:jboss-test-adapter");
   }

   public Connection connect(String url, Properties info) throws SQLException
   {
      return new TestConnection(this);
   }

   public int getMajorVersion()
   {
      return 1;
   }

   public int getMinorVersion()
   {
      return 0;
   }

   public DriverPropertyInfo[] getPropertyInfo(String url, Properties info)
   {
      return null;
   }

   public boolean jdbcCompliant()
   {
      return false;
   }

}// TestDriver
