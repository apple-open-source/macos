package org.jboss.test.cts.ejb;

import java.lang.reflect.InvocationTargetException;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EJBObject;
import javax.naming.InitialContext;

import org.apache.log4j.Logger;

import org.jboss.test.cts.interfaces.StatelessSessionHome;
import org.jboss.test.cts.interfaces.StatelessSession;
import org.jboss.test.cts.interfaces.ClientCallback;
import org.jboss.test.cts.interfaces.StatelessSessionLocalHome;
import org.jboss.test.cts.interfaces.StatelessSessionLocal;
import org.jboss.test.util.ejb.SessionSupport;

/** The stateless session bean implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.10.2.1 $
 */
public class StatelessSessionBean
      extends SessionSupport
{
   private static Logger log = Logger.getLogger(StatelessSessionBean.class);

   public void ejbCreate()
         throws CreateException
   {
   }

   public String method1(String msg)
   {
      return msg;
   }

   public void loopbackTest()
         throws java.rmi.RemoteException
   {
      try
      {
         InitialContext ctx = new InitialContext();
         StatelessSessionHome home = (StatelessSessionHome) ctx.lookup("ejbcts/StatelessSessionBean");
         StatelessSession sessionBean;
         try
         {
            sessionBean = home.create();
         }
         catch (CreateException ex)
         {
            log.debug("Loopback CreateException: " + ex);
            throw new EJBException(ex);
         }
         sessionBean.loopbackTest(sessionCtx.getEJBObject());
      }
      catch (javax.naming.NamingException nex)
      {
         log.debug("Could not locate bean instance");
      }
   }

   public void loopbackTest(EJBObject obj)
         throws java.rmi.RemoteException
   {
      // This should throw an exception.
      StatelessSession bean = (StatelessSession) obj;
      bean.method1("Hello");
   }

   public void callbackTest(ClientCallback callback, String data)
         throws java.rmi.RemoteException
   {
      callback.callback(data);
   }

   public void npeError()
   {
      Object obj = null;
      obj.toString();
   }

   public void testLocalHome() throws InvocationTargetException
   {
      StatelessSessionLocalHome home = (StatelessSessionLocalHome) sessionCtx.getEJBLocalHome();
      log.debug("Obtained StatelessSessionLocalHome from ctx");
      try
      {
         StatelessSessionLocal local = home.create();
         log.debug("Created StatelessSessionLocal#1");
         StatelessSessionLocalHome home2 = (StatelessSessionLocalHome) local.getEJBLocalHome();
         log.debug("Obtained StatelessSessionLocalHome from StatelessSessionLocal");
         local = home2.create();
         log.debug("Created StatelessSessionLocal#2");
         local.remove();
      }
      catch(Exception e)
      {
         log.debug("testLocalHome failed", e);
         throw new InvocationTargetException(e, "testLocalHome failed");
      }
   }
}
