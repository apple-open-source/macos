package org.jboss.test.security.ejb;

import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.apache.log4j.Logger;

/** A simple session bean for testing declarative security.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.6.1 $
 */
public class StatefulSessionBean implements SessionBean
{
   private static Logger log = Logger.getLogger(StatefulSessionBean.class);
   private SessionContext sessionContext;
   private String state;

   public void ejbCreate(String state) throws CreateException
   {
      this.state = state;
      log.debug("ejbCreate("+state+") called");
      Principal p = sessionContext.getCallerPrincipal();
      log.debug("ejbCreate, callerPrincipal="+p);
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
      return arg;
   }
}
