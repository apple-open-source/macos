package org.jboss.test.web.ejb;

import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.jboss.logging.Logger;
import org.jboss.test.web.interfaces.ReferenceTest;
import org.jboss.test.web.interfaces.ReturnData;

/** A simple session bean for testing declarative security.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.5.4.2 $
 */
public class StatelessSessionBean implements SessionBean
{
   static Logger log = Logger.getLogger(StatelessSessionBean.class);

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
      log.debug("echo, arg=" + arg);
      Principal p = sessionContext.getCallerPrincipal();
      log.debug("echo, callerPrincipal=" + p);
      return p.getName();
   }

   public String forward(String echoArg)
   {
      log.debug("StatelessSessionBean2.forward, echoArg=" + echoArg);
      return echo(echoArg);
   }

   public void noop(ReferenceTest test, boolean optimized)
   {
      log.debug("noop");
   }

   public ReturnData getData()
   {
      ReturnData data = new ReturnData();
      data.data = "TheReturnData";
      return data;
   }

   /** A method deployed with no method permissions */
   public void unchecked()
   {
      log.debug("unchecked");      
   }

   /** A method deployed with method permissions such that only a run-as
    * assignment will allow access. 
    */
   public void checkRunAs()
   {
      Principal caller = sessionContext.getCallerPrincipal();
      log.debug("checkRunAs, caller="+caller);
      boolean isInternalUser = sessionContext.isCallerInRole("InternalUser");
      log.debug("Caller isInternalUser: "+isInternalUser);
   }
}
