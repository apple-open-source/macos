package org.jboss.test.security.ejb;

import java.rmi.RemoteException;
import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.apache.log4j.Category;

/** A simple session bean that is called by the CallerBean to test
 run-as identity and role propagation.
 
 @author Scott.Stark@jboss.org
 @version $Revision: 1.2 $
 */
public class CalleeBean implements SessionBean
{
   private static Category log = Category.getInstance(CalleeBean.class);
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
      isCaller = sessionContext.isCallerInRole("InternalRole");
      log.debug("echo, isCallerInRole('InternalRole')="+isCaller);
      return arg;
   }

   public void noop()
   {
      log.debug("noop");
   }   
}
