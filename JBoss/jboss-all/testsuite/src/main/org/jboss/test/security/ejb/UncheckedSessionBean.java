package org.jboss.test.security.ejb;

import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.apache.log4j.Logger;

/** A simple session bean for testing unchecked declarative security.

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.2.1 $
 */
public class UncheckedSessionBean implements SessionBean
{
   Logger log = Logger.getLogger(getClass());

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
      boolean isCaller = sessionContext.isCallerInRole("EchoCaller");
      log.debug("echo, isCallerInRole('EchoCaller')=" + isCaller);
      return arg;
   }

   public String forward(String echoArg)
   {
      log.debug("forward, echoArg=" + echoArg);
      return echo(echoArg);
   }

   public void noop()
   {
      log.debug("noop");
   }

   public void npeError()
   {
      log.debug("npeError");
      Object obj = null;
      obj.toString();
   }

   public void unchecked()
   {
      Principal p = sessionContext.getCallerPrincipal();
      log.debug("unchecked, callerPrincipal=" + p);
   }

   public void excluded()
   {
      throw new EJBException("excluded, no access should be allowed");
   }

}
