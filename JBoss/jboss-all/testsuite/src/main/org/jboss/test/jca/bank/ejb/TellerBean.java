
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.test.jca.bank.ejb;

import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.*;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.sql.DataSource;
import org.apache.log4j.Category;
import org.jboss.test.jca.bank.interfaces.Account;
import org.jboss.test.jca.bank.interfaces.AccountHome;
import org.jboss.test.jca.bank.interfaces.AccountLocal;
import org.jboss.test.jca.bank.interfaces.AccountLocalHome;
import org.jboss.logging.Logger;


/**
 * Describe class <code>TellerBean</code> here.
 * The equals and hashCode methods have been overridden to test bug 595738,
 * a problem  with CachedConnectionManager if it directly puts objects in a hashmap.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version 1.0
 *
 * @ejb:bean   name="Teller"
 *             jndi-name="Teller"
 *             local-jndi-name="LocalTellerBean"
 *             view-type="both"
 *             type="Stateless"
 */
public class TellerBean
   implements SessionBean
{
   private static Logger log = Logger.getLogger(TellerBean.class);
   static int invocations;

   private Connection c;

   /**
    * Describe <code>setUp</code> method here.
    *
    * @exception EJBException if an error occurs
    * @ejb:interface-method
    */
   public void setUp()
   {
      try
      {
         tearDown();
      }
      catch (Exception e)
      {
         //ignore
      } // end of try-catch

      try
      {
         Statement s = getConnection().createStatement();
         s.execute("CREATE TABLE CCBMPCUSTOMER (ID INTEGER NOT NULL PRIMARY KEY, NAME VARCHAR(64))");
         s.execute("CREATE TABLE CCBMPACCOUNT (ID INTEGER NOT NULL PRIMARY KEY, BALANCE INTEGER NOT NULL, CUSTOMERID INTEGER)");
         s.close();

      }
      catch (Exception e)
      {
         throw new EJBException(e);
      } // end of try-catch

   }

   /**
    * Describe <code>tearDown</code> method here.
    *
    * @exception EJBException if an error occurs
    * @ejb:interface-method
    */
   public void tearDown()
   {
      try
      {
         Statement s = getConnection().createStatement();
         s.execute("DROP TABLE CCBMPCUSTOMER");
         s.execute("DROP TABLE CCBMPACCOUNT");
         s.close();

      }
      catch (Exception e)
      {
         throw new EJBException(e);
      } // end of try-catch

   }

   /**
    * This <code>equals</code> method tests whether the CachedConnectionManager deals
    * properly with ejbs that override equals and hashCode.  See bug 595738
    *
    * @param other an <code>Object</code> value
    * @return a <code>boolean</code> value
    */
   public boolean equals(Object other)
   {
      return other.getClass() == this.getClass();
   }

   public int hashCode()
   {
      return 1;
   }

   /**
    * Describe <code>transfer</code> method here.
    *
    * @param from an <code>Account</code> value
    * @param to an <code>Account</code> value
    * @param amount a <code>float</code> value
    * @exception EJBException if an error occurs
    * @ejb:interface-method
    */
   public void transfer(Account from, Account to, int amount)
   {
      try
      {
         log.debug("Invocation #"+invocations++);
         from.withdraw(amount);
         to.deposit(amount);
      } catch (Exception e)
      {
         throw new EJBException("Could not transfer "+amount+" from "+from+" to "+to, e);
      }
   }

   /**
    * Describe <code>createAccount</code> method here.
    *
    * @param id a <code>Integer</code> value, id of account
    * @return an <code>Account</code> value
    * @exception EJBException if an error occurs
    * @ejb:interface-method
    */
   public Account createAccount(Integer id)
   {
      try
      {
         AccountHome home = (AccountHome)new InitialContext().lookup("Account");
         Account acct = home.create(id, 0, null);

         return acct;
      } catch (Exception e)
      {
         throw new EJBException("Could not create account", e);
      }
   }

   /**
    * Describe <code>getAccountBalance</code> method here.
    *
    * @param id a <code>integer</code> value, id of account
    * @return an <code>int</code> value, balbance of account
    * @exception EJBException if an error occurs
    * @ejb:interface-method
    */
   public int getAccountBalance(Integer id)
   {
      try
      {
         AccountLocalHome home = (AccountLocalHome)new InitialContext().lookup("AccountLocal");
         AccountLocal a = home.findByPrimaryKey(id);
         return a.getBalance();
      } catch (Exception e)
      {
         Category.getInstance(getClass().getName()).info("getAccountBalance failed", e);
         throw new EJBException("Could not get account for id " + id, e);
      }
   }


   /**
    * Describe <code>transferTest</code> method here.
    *
    * @param from an <code>AccountLocal</code> value
    * @param to an <code>AccountLocal</code> value
    * @param amount a <code>float</code> value
    * @param iter an <code>int</code> value
    * @exception java.rmi.RemoteException if an error occurs
    * @exception EJBException if an error occurs
    * @ejb:interface-method
    */
   public void transferTest(AccountLocal from, AccountLocal to, int amount, int iter)
   {
      for (int i = 0; i < iter; i++)
      {
         from.withdraw(amount);
         to.deposit(amount);
      }
   }

   public void ejbCreate()
   {
   }

   public void ejbActivate()
   {
   }

   public void ejbPassivate()
   {
      if (c != null)
      {
         try
         {
            c.close();
         }
         catch (SQLException e)
         {
            Category.getInstance(getClass().getName()).info("SQLException closing c: " + e);
         } // end of try-catch
         c = null;
      } // end of if ()
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

   private Connection getConnection() throws Exception
   {
      if (c == null)
      {
         DataSource ds = (DataSource)new InitialContext().lookup("java:/DefaultDS");
         c = ds.getConnection();

      } // end of if ()

      return c;
   }
}
