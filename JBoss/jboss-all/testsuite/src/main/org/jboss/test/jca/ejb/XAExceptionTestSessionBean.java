
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.ejb;




import java.rmi.RemoteException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.ejb.TransactionRolledbackLocalException;
import javax.naming.InitialContext;
import javax.transaction.TransactionRolledbackException;
import org.apache.log4j.Logger;
import org.jboss.test.jca.interfaces.XAExceptionSession;
import org.jboss.test.jca.interfaces.XAExceptionSessionHome;
import org.jboss.test.jca.interfaces.XAExceptionSessionLocal;
import org.jboss.test.jca.interfaces.XAExceptionSessionLocalHome;

/**
 * XAExceptionTestSessionBean.java
 *
 *
 * Created: Wed Sep 11 00:38:00 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="XAExceptionTestSession"
 *             jndi-name="test/XAExceptionTestSessionHome"
 *             view-type="remote"
 *             type="Stateless"
 * @ejb.transaction type="Never"
 *
 */

public class XAExceptionTestSessionBean implements SessionBean  {

   private Logger log = Logger.getLogger(getClass().getName());

   /**
    * Describe <code>ejbCreate</code> method here.
    * @ejb.interface-method
    */
   public void ejbCreate()
   {
   }


   /**
    * Describe <code>testXAException</code> method here.
    * @ejb.interface-method
    */
   public void testXAExceptionToTransactionRolledbackException()
   {
      try 
      {
         XAExceptionSessionHome xh = (XAExceptionSessionHome)new InitialContext().lookup("test/XAExceptionSessionHome");
         XAExceptionSession x = xh.create();
         try 
         {
            x.testXAExceptionToTransactionRolledbackException();
         }
         catch (TransactionRolledbackException tre)
         {
            System.out.println("Test worked");
            return;
         } // end of try-catch
      }
      catch (Exception e)
      {
         log.info("unexpected exception", e);
         throw new EJBException("Unexpected exception: " + e);
      } // end of try-catch
      throw new EJBException("No exception");
   }

   /**
    * Describe <code>testXAException</code> method here.
    * @ejb.interface-method
    */
   public void testXAExceptionToTransactionRolledbackLocalException()
   {
      try 
      {
         XAExceptionSessionLocalHome xh = (XAExceptionSessionLocalHome)new InitialContext().lookup("test/XAExceptionSessionLocalHome");
         XAExceptionSessionLocal x = xh.create();
         try 
         {
            x.testXAExceptionToTransactionRolledbackException();
         }
         catch (TransactionRolledbackLocalException tre)
         {
            log.info("Test worked");
            return;
         } // end of try-catch
      }
      catch (Exception e)
      {
         log.info("unexpected exception", e);
         throw new EJBException("Unexpected exception: " + e);
      } // end of try-catch
      throw new EJBException("No exception");
   }

   /**
    * Describe <code>testXAException</code> method here.
    * @ejb.interface-method
    */
   public void testRMERRInOnePCToTransactionRolledbackException()
   {
      try 
      {
         XAExceptionSessionHome xh = (XAExceptionSessionHome)new InitialContext().lookup("test/XAExceptionSessionHome");
         XAExceptionSession x = xh.create();
         try 
         {
            x.testRMERRInOnePCToTransactionRolledbackException();
         }
         catch (TransactionRolledbackException tre)
         {
            System.out.println("Test worked");
            return;
         } // end of try-catch
      }
      catch (Exception e)
      {
         log.info("unexpected exception", e);
         throw new EJBException("Unexpected exception: " + e);
      } // end of try-catch
      throw new EJBException("No exception");
   }

   /**
    * Describe <code>testXAException</code> method here.
    * @ejb.interface-method
    */
   public void testXAExceptionToTransactionRolledbacLocalkException()
   {
      try 
      {
         XAExceptionSessionLocalHome xh = (XAExceptionSessionLocalHome)new InitialContext().lookup("test/XAExceptionSessionLocalHome");
         XAExceptionSessionLocal x = xh.create();
         try 
         {
            x.testRMERRInOnePCToTransactionRolledbackException();
         }
         catch (TransactionRolledbackLocalException tre)
         {
            log.info("Test worked");
            return;
         } // end of try-catch
      }
      catch (Exception e)
      {
         log.info("unexpected exception", e);
         throw new EJBException("Unexpected exception: " + e);
      } // end of try-catch
      throw new EJBException("No exception");
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

}

