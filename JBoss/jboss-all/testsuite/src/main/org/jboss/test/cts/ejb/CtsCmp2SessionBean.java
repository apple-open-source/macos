package org.jboss.test.cts.ejb;

import java.lang.reflect.Method;
import java.rmi.RemoteException;
import java.rmi.ServerException;
import javax.naming.InitialContext;

import org.jboss.test.cts.interfaces.CtsCmp2Local;
import org.jboss.test.cts.interfaces.CtsCmp2LocalHome;
import org.jboss.test.util.ejb.SessionSupport;

/**
 * Class StatelessSessionBean
 *
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 */
public class CtsCmp2SessionBean extends SessionSupport
{

   public void ejbCreate ()
   {
   }

   public void testV1() throws RemoteException
   {
      try
      {
         InitialContext ctx = new InitialContext();
         CtsCmp2LocalHome home = (CtsCmp2LocalHome) ctx.lookup("java:comp/env/ejb/CtsCmp2LocalHome");
         CtsCmp2Local bean = home.create("key1", "data1");
         String data = bean.getData();
         bean.remove();
      }
      catch(Exception e)
      {
         throw new ServerException("testV1 failed", e);
      }
   }
   public void testV2() throws RemoteException
   {
      try
      {
         InitialContext ctx = new InitialContext();
         CtsCmp2LocalHome home = (CtsCmp2LocalHome) ctx.lookup("java:comp/env/ejb/CtsCmp2LocalHome");
         CtsCmp2Local bean = home.create("key1", "data1");
         String data = bean.getData();
         Class[] sig = {};
         Method getMoreData = bean.getClass().getMethod("getMoreData", sig);
         Object[] args = {};
         data = (String) getMoreData.invoke(bean, args);
         bean.remove();
      }
      catch(Exception e)
      {
         throw new ServerException("testV2 failed", e);
      }
   }
}
