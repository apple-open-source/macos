/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jca.ejb;

import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import org.apache.log4j.Category;
import javax.transaction.UserTransaction;
import javax.ejb.EJBException;

import org.jboss.test.jca.adapter.TestConnectionFactory;
import org.jboss.test.jca.adapter.TestConnection;


/**
 * UserTxSessionBean.java
 *
 *
 * Created: Thu Jun 27 09:02:18 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 * @ejb.bean   name="UserTxSession"
 *             jndi-name="UserTxSession"
 *             local-jndi-name="LocalUserTxSession"
 *             view-type="both"
 *             type="Stateless"
 *             transaction-type="Bean"
 * @ejb.transaction type="NotSupported"
 */

public class UserTxSessionBean 
   implements SessionBean  
{

   private SessionContext ctx;
   private Category log = Category.getInstance(getClass().getName());

   public UserTxSessionBean() 
   {
      
   }
   
   /**
    * Describe <code>testUserTxJndi</code> method here.
    *
    * @return a <code>boolean</code> value
    *
    * @ejb:interface-method
    */
   public boolean testUserTxJndi()
   {
      try 
      {
         TestConnectionFactory tcf = (TestConnectionFactory)new InitialContext().lookup("java:/JBossTestCF");
         TestConnection tc = (TestConnection)tcf.getConnection();
         UserTransaction ut = (UserTransaction)new InitialContext().lookup("UserTransaction");
         ut.begin();
         boolean result = tc.isInTx();
         log.info("Jndi test, inTx: " + result);
         ut.commit();
         tc.close();
         return result;
      }
      catch (Exception e)
      {
         throw new EJBException(e.getMessage());
      } // end of try-catch
      
   }

   /**
    * Describe <code>testUserTxSessionCtx</code> method here.
    *
    * @return a <code>boolean</code> value
    *
    * @ejb:interface-method
    */
   public boolean testUserTxSessionCtx()
   {
      try 
      {
         TestConnectionFactory tcf = (TestConnectionFactory)new InitialContext().lookup("java:/JBossTestCF");
         TestConnection tc = (TestConnection)tcf.getConnection();
         UserTransaction ut = ctx.getUserTransaction();
         ut.begin();
         boolean result = tc.isInTx();
         log.info("ctx test, inTx: " + result);
         ut.commit();
         tc.close();
         return result;
      }
      catch (Exception e)
      {
         throw new EJBException(e.getMessage());
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
      this.ctx = ctx;
   }

   public void unsetSessionContext()
   {
      this.ctx = null;
   }

}// UserTxSessionBean
