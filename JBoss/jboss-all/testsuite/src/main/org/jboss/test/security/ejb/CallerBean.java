package org.jboss.test.security.ejb;

import java.rmi.RemoteException;
import java.rmi.ServerException;
import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import org.apache.log4j.Category;
import org.jboss.test.security.interfaces.StatelessSessionLocal;
import org.jboss.test.security.interfaces.StatelessSessionLocalHome;

/** A simple session bean that calls the CalleeBean
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public class CallerBean implements SessionBean
{
   private static Category log = Category.getInstance(CallerBean.class);
   private SessionContext sessionContext;

   public void ejbCreate() throws CreateException
   {
      log.debug("ejbCreate() called");
   }

   public void ejbActivate()
   {
      log.debug("ejbActivate() called");
   }

   public void ejbPassivate()
   {
      log.debug("ejbPassivate() called");
   }

   public void ejbRemove()
   {
      log.debug("ejbRemove() called");
   }

   public void setSessionContext(SessionContext context)
   {
      sessionContext = context;
   }

   public String echo(String arg)
   {
      log.debug("echo, arg="+arg);
      Principal p = sessionContext.getCallerPrincipal();
      log.debug("echo, callerPrincipal="+p);
      boolean isCaller = sessionContext.isCallerInRole("EchoCaller");
      log.debug("echo, isCallerInRole('EchoCaller')="+isCaller);
      try
      {
         InitialContext ic = new InitialContext();
         Context enc = (Context) ic.lookup("java:comp/env");
         Object ref = enc.lookup("ejb/local/CalleeHome");
         StatelessSessionLocalHome localHome = (StatelessSessionLocalHome) PortableRemoteObject.narrow(ref,
               StatelessSessionLocalHome.class);
         StatelessSessionLocal localBean = localHome.create();
         String echo2 = localBean.echo(arg);
        log.debug("echo, callee.echo="+echo2);
      }
      catch(Exception e)
      {
         log.error("Failed to invoke Callee.echo", e);
         throw new EJBException("Failed to invoke Callee.echo", e);
      }
      return arg;
   }

   public void noop()
   {
      log.debug("noop");
   }
   
}
