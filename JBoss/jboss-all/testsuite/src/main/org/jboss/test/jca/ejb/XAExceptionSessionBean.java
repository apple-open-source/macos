
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
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.resource.ResourceException;
import javax.resource.cci.Connection;
import javax.resource.cci.ConnectionFactory;
import javax.transaction.xa.XAException;
import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.test.jca.adapter.TestConnection;
import org.jboss.test.jca.adapter.TestConnectionFactory;

/**
 * XAExceptionSessionBean.java
 *
 *
 * Created: Tue Sep 10 21:34:30 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="XAExceptionSession"
 *             jndi-name="test/XAExceptionSessionHome"
 *             local-jndi-name="test/XAExceptionSessionLocalHome"
 *             view-type="both"
 *             type="Stateless"
 * @ejb.transaction type="Required"
 *
 */

public class XAExceptionSessionBean implements SessionBean  {

   private SessionContext sessionContext;

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
      
      InitialContext ctx = new InitialContext();
      ConnectionFactory cf1 = (ConnectionFactory)ctx.lookup("java:/JBossTestCF");        
      ConnectionFactory cf2 = (ConnectionFactory)ctx.lookup("java:/JBossTestCF2");        
      Connection c1 = cf1.getConnection();
      try 
      {
         TestConnection c2 = (TestConnection)cf2.getConnection();            
         try 
         {
            c2.setFailInPrepare(true, XAException.XA_RBROLLBACK);
         }
         finally
         {
            c2.close();
         } // end of try-catch
         

      }
      finally
      {
         c1.close();
      } // end of try-catch
          
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new EJBException("unexpected exception: " + e);
      } // end of try-catch
   }


   /**
    * Describe <code>testXAException</code> method here.
    * @ejb.interface-method
    */
   public void testRMERRInOnePCToTransactionRolledbackException()
   {
      try 
      {
      
      InitialContext ctx = new InitialContext();
      ConnectionFactory cf1 = (ConnectionFactory)ctx.lookup("java:/JBossTestCF");        
      TestConnection c1 = (TestConnection)cf1.getConnection();
      try 
      {
	 c1.setFailInCommit(true, XAException.XAER_RMERR);
         

      }
      finally
      {
         c1.close();
      } // end of try-catch
          
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new EJBException("unexpected exception: " + e);
      } // end of try-catch
   }

   /**
    * Similate a connection failure
    *
    * @ejb.interface-method
    */
   public void simulateConnectionError()
   {
      System.out.println("Simulating connection error");
      try
      {
         InitialContext ctx = new InitialContext();
         ConnectionFactory cf = (ConnectionFactory) ctx.lookup("java:/JBossTestCF");
         TestConnection c = (TestConnection) cf.getConnection();
         try
         {
            c.simulateConnectionError();
         }
         finally
         {
            c.close();
         }
      }
      catch (Exception e)
      {
         if (e.getMessage().equals("Simulated exception") == false)
         {
            e.printStackTrace();
            throw new EJBException(e.getMessage());
         }
         else
         {
            sessionContext.setRollbackOnly();
         }
      }
   }

   /**
    * Similate a connection failure
    *
    * @ejb.interface-method
    */
   public void simulateConnectionErrorWithTwoHandles()
   {
      System.out.println("Simulating connection error with two handles");
      try
      {
         InitialContext ctx = new InitialContext();
         ConnectionFactory cf = (ConnectionFactory) ctx.lookup("java:/JBossTestCFByTx");
         TestConnection c1 = (TestConnection) cf.getConnection();
         TestConnection c2 = (TestConnection) cf.getConnection();
         try
         {
            c2.simulateConnectionError();
         }
         finally
         {
            try
            {
               c1.close();
            }
            catch (Throwable ignored)
            {
            }
            try
            {
               c2.close();
            }
            catch (Throwable ignored)
            {
            }
         }
      }
      catch (Exception e)
      {
         if (e.getMessage().equals("Simulated exception") == false)
         {
            e.printStackTrace();
            throw new EJBException(e.getMessage());
         }
         else
         {
            sessionContext.setRollbackOnly();
         }
      }
   }

   /**
    * Similate an exception
    *
    * @ejb.interface-method
    */
   public void simulateError(String failure, int count)
   {
      System.out.println(failure + " start");
      try
      {
         long available = getAvailableConnections();
         InitialContext ctx = new InitialContext();
         TestConnectionFactory cf = (TestConnectionFactory) ctx.lookup("java:/JBossTestCF");
         for (int i = 0; i < count; ++i)
         {
            try
            {
               TestConnection c = (TestConnection) cf.getConnection(failure);
               c.close();
            }
            catch (ResourceException expected)
            {
            }
         }
         if (available != getAvailableConnections())
            throw new EJBException("Expected " + available + 
                " got " + getAvailableConnections() + " connections");
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new EJBException(e.getMessage());
      }
      finally
      {
         flushConnections();
      }
   }

   /**
    * Similate an exception
    *
    * @ejb.interface-method
    */
   public void simulateFactoryError(String failure, int count)
   {
      System.out.println(failure + " start");
      TestConnectionFactory cf = null;
      try
      {
         long available = getAvailableConnections();
         InitialContext ctx = new InitialContext();
         cf = (TestConnectionFactory) ctx.lookup("java:/JBossTestCF");
         cf.setFailure(failure);
         for (int i = 0; i < count; ++i)
         {
            try
            {
               TestConnection c = (TestConnection) cf.getConnection(failure);
               c.close();
            }
            catch (ResourceException expected)
            {
            }
         }
         if (available != getAvailableConnections())
            throw new EJBException("Expected " + available + 
                " got " + getAvailableConnections() + " connections");
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new EJBException(e.getMessage());
      }
      finally
      {
         if (cf != null)
            failure = null;
         flushConnections();
      }
   }
   public long getAvailableConnections()
      throws Exception
   {
      MBeanServer server = MBeanServerLocator.locateJBoss();
      return ((Long) server.getAttribute(new ObjectName("jboss.jca:service=ManagedConnectionPool,name=JBossTestCF"), 
             "AvailableConnectionCount")).longValue();
   }

   public void flushConnections()
   {
      try
      {
         MBeanServer server = MBeanServerLocator.locateJBoss();
         server.invoke(new ObjectName("jboss.jca:service=ManagedConnectionPool,name=JBossTestCF"), "flush", new Object[0], new String[0]);
      }
      catch (Exception e)
      {
         throw new EJBException(e);
      }
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
      sessionContext = ctx;
   }

   public void unsetSessionContext()
   {
      sessionContext = null;
   }

}

