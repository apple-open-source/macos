package org.jboss.test.invokers.ejb;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;

import org.jboss.test.invokers.interfaces.SimpleBMP;
import org.jboss.test.invokers.interfaces.SimpleBMPHome;

/** A simple session bean for testing access over custom RMI sockets.

@author Scott_Stark@displayscape.com
@version $Revision: 1.1 $
*/
public class StatelessSessionBean implements SessionBean
{
   static org.apache.log4j.Category log =
      org.apache.log4j.Category.getInstance(StatelessSessionBean.class);
   
   private SessionContext sessionContext;

   public void ejbCreate() throws CreateException
   {
      log.debug("StatelessSessionBean.ejbCreate() called");
   }

   public void ejbActivate()
   {
      log.debug("StatelessSessionBean.ejbActivate() called");
   }

   public void ejbPassivate()
   {
      log.debug("StatelessSessionBean.ejbPassivate() called");
   }

   public void ejbRemove()
   {
      log.debug("StatelessSessionBean.ejbRemove() called");
   }

   public void setSessionContext(SessionContext context)
   {
      sessionContext = context;
   }

   public SimpleBMP getBMP(int id) throws RemoteException
   {
      try
      {
         InitialContext ctx = new InitialContext();
         SimpleBMPHome home = (SimpleBMPHome)ctx.lookup("java:comp/env/ejb/SimpleBMP");
         return home.findByPrimaryKey(new Integer(id));
      }
      catch (Exception ex)
      {
         ex.printStackTrace();
         throw new RemoteException("error");
      }
   }

}
