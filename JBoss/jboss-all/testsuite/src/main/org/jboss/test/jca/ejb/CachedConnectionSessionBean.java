
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.ejb;




import java.rmi.RemoteException;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.sql.DataSource;
import org.apache.log4j.Category;
import org.jboss.test.jca.interfaces.CachedConnectionSessionLocal;

/**
 * CachedConnectionSessionBean.java
 *
 *
 * Created: Sun Mar 10 17:55:51 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="CachedConnectionSession"
 *             jndi-name="CachedConnectionSession"
 *             local-jndi-name="CachedConnectionSessionBean"
 *             view-type="both"
 *             type="Stateless"
 *
 */

public class CachedConnectionSessionBean implements SessionBean  {

   private Connection conn;
   private Category log = Category.getInstance(getClass().getName());
   private SessionContext ctx;

   /**
    * Describe <code>createTable</code> method here.
    *
    * @ejb:interface-method
    */
   public void createTable()
   {
      try
      {
         dropTable();
      }
      catch (Exception e)
      {
         //ignore
      } // end of try-catch

      try
      {
         Statement s = getConn().createStatement();
         try
         {
            s.execute("CREATE TABLE TESTCACHEDCONN (ID NUMERIC(18,0) NOT NULL PRIMARY KEY, VAL VARCHAR(255))");
         }
         finally
         {
            s.close();
         } // end of try-catch
      }
      catch (SQLException e)
      {
         log.error("sql exception in create table", e);
      } // end of try-catch
   }

   /**
    * Describe <code>dropTable</code> method here.
    *
    * @ejb:interface-method
    */
   public void dropTable()
   {
      try
      {
         Statement s = getConn().createStatement();
         try
         {
            s.execute("DROP TABLE TESTCACHEDCONN");
         }
         finally
         {
            s.close();
         } // end of try-catch
      }
      catch (SQLException e)
      {
         log.error("sql exception in drop", e);
      } // end of try-catch
   }

   /**
    * Describe <code>insert</code> method here.
    *
    * @param id a <code>String</code> value
    * @param value a <code>String</code> value
    *
    * @ejb:interface-method
    */
   public void insert(long id, String value)
   {
      try
      {
         PreparedStatement p = getConn().prepareStatement("INSERT INTO TESTCACHEDCONN (ID, VAL) VALUES (?, ?)");
         try
         {
            p.setLong(1, id);
            p.setString(2, value);
            p.execute();
         }
         finally
         {
            p.close();
         } // end of try-catch
      }
      catch (SQLException e)
      {
         log.error("sql exception in insert", e);
      } // end of try-catch
   }

   /**
    * Describe <code>fetch</code> method here.
    *
    * @param id a <code>String</code> value
    *
    * @ejb:interface-method
    */
   public String fetch(long id)
   {
      try
      {
         PreparedStatement p = getConn().prepareStatement("SELECT VAL FROM TESTCACHEDCONN WHERE ID = ?");
         ResultSet rs = null;
         try
         {
            p.setLong(1, id);
            rs = p.executeQuery();
            if (rs.next())
            {
               return rs.getString(1);
            } // end of if ()
            return null;
         }
         finally
         {
            rs.close();
            p.close();
         } // end of try-catch
      }
      catch (SQLException e)
      {
         log.error("sql exception in fetch", e);
         return null;
      } // end of try-catch
   }
   private Connection getConn()
   {
      if (conn == null)
      {
         log.info("ejb activate never called, conn == null");
         ejbActivate();
      } // end of if ()
      if (conn == null)
      {
         throw new IllegalStateException("could not get a connection");
      } // end of if ()

      return conn;
   }

   /**
    * Invoke another bean that opens a thread local connection,
    * we close it.
    *
    * @ejb:interface-method
    */
   public void firstTLTest()
   {
      try
      {
         CachedConnectionSessionLocal other = (CachedConnectionSessionLocal) ctx.getEJBLocalObject();
         other.secondTLTest();
         ThreadLocalDB.close();
      }
      catch (Exception e)
      {
         log.info("Error", e);
         throw new EJBException(e.toString());
      }
   }

   /**
    * @ejb:interface-method
    */
   public void secondTLTest()
   {
      try
      {
         Connection c = ThreadLocalDB.open();
         c.createStatement().close();
      }
      catch (Exception e)
      {
         log.info("Error", e);
         throw new EJBException(e.toString());
      }
   }

   public void ejbCreate()
   {
   }

   public void ejbActivate()
   {
      log  = Category.getInstance(getClass());
      try
      {
         //DataSource ds = (DataSource)new InitialContext().lookup("java:/comp/env/datasource");
         DataSource ds = (DataSource)new InitialContext().lookup("java:DefaultDS");
         conn = ds.getConnection();
      }
      catch (NamingException e)
      {
         log.error("naming exception in activate", e);
      } // end of try-catch
      catch (SQLException e)
      {
         log.error("sql exception in activate", e);
      } // end of try-catch

    }

   public void ejbPassivate() throws RemoteException
   {
      try
      {
         conn.close();
      }
      catch (SQLException e)
      {
         log.error("sql exception in passivate", e);
      } // end of try-catch
      conn = null;
      log = null;
   }

   public void ejbRemove() throws RemoteException
   {
   }

   public void setSessionContext(SessionContext ctx) throws RemoteException
   {
      this.ctx = ctx;
   }

   public void unsetSessionContext() throws RemoteException
   {
   }

}

