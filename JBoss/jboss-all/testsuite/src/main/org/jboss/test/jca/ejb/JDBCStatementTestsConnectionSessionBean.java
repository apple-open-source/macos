/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.ejb;

import javax.ejb.SessionBean;
import javax.naming.InitialContext;
import javax.sql.DataSource;
import java.sql.Connection;
import java.sql.SQLException;
import java.security.CodeSource;
import javax.ejb.EJBException;
import javax.ejb.SessionContext;
import javax.naming.NamingException;

import org.jboss.resource.adapter.jdbc.WrappedConnection;
import org.jboss.test.jca.jdbc.TestConnection;
import org.jboss.logging.Logger;

/**
 * JDBCStatementTestsConnectionSessionBean.java
 *
 *
 * Created: Fri Feb 14 15:01:38 2003
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 * @ejb:bean   name="JDBCStatementTestsConnectionSession"
 *             jndi-name="JDBCStatementTestsConnectionSession"
 *             local-jndi-name="JDBCStatementTestsConnectionSessionLocal"
 *             view-type="both"
 *             type="Stateless"
 */

public class JDBCStatementTestsConnectionSessionBean implements SessionBean
{
   private static final Logger log = Logger.getLogger(JDBCStatementTestsConnectionSessionBean.class);

   public JDBCStatementTestsConnectionSessionBean()
   {
   }

   /**
    * The <code>testConnectionObtainable</code> method gets
    * connections from the TestDriver after setting fail to true.
    * This causes the test sql to throw an exception when the
    * connection is retrieved from a pool, which closes the
    * connection, forcing the connectionmanager to get a new one.  We
    * check this by counting how many connections have been closed.
    *
    *
    * @ejb:interface-method
    */
   public void testConnectionObtainable()
   {
      try
      {
         DataSource ds = (DataSource)new InitialContext().lookup("java:StatementTestsConnectionDS");
         Connection c = ds.getConnection();
         WrappedConnection wc = (WrappedConnection)c;
         Connection uc = wc.getUnderlyingConnection();
         CodeSource cs1 = TestConnection.class.getProtectionDomain().getCodeSource();
         CodeSource cs2 = uc.getClass().getProtectionDomain().getCodeSource();
         log.debug("CS1: "+cs1);
         log.debug("CS2: "+cs2);
         TestConnection tc = (TestConnection) uc;
         c.close();
         tc.setFail(true);
         int closeCount1 = tc.getClosedCount();
         c = ds.getConnection();
         if (closeCount1 == tc.getClosedCount())
         {
            throw new EJBException("no connections closed!, closedCount: " + closeCount1);
         } // end of if ()
         c.close();
         for (int i = 0; i < 10; i++)
         {

            int closeCount = tc.getClosedCount();
            c = ds.getConnection();
            if (closeCount == tc.getClosedCount())
            {
               throw new EJBException("no connections closed! at iteration: " + i + ", closedCount: " + closeCount);
            } // end of if ()
            c.close();
         } // end of for ()

      }
      catch (SQLException e)
      {
         throw new EJBException(e);
      } // end of try-catch
      catch (NamingException e)
      {
         throw new EJBException(e);
      } // end of try-catch

   }

   public void ejbCreate()
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
   }

   public void ejbRemove()
   {
   }

   public void setSessionContext(SessionContext ctx)
   {
   }

   public void unsetSessionContext()
   {
   }

}// JDBCStatementTestsConnectionSessionBean
