package org.jboss.test.web.ejb;

import java.rmi.RemoteException;
import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.jboss.test.web.interfaces.ReferenceTest;
import org.jboss.test.web.interfaces.ReturnData;

/** A simple session bean for testing declarative security.

@author Scott.Stark@jboss.org
@version $Revision: 1.5.4.1 $
*/
public class StatelessSessionBean implements SessionBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
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

    public String echo(String arg)
    {
        log.debug("StatelessSessionBean.echo, arg="+arg);
        Principal p = sessionContext.getCallerPrincipal();
        log.debug("StatelessSessionBean.echo, callerPrincipal="+p);
        return p.getName();
    }
    public String forward(String echoArg)
    {
        log.debug("StatelessSessionBean2.forward, echoArg="+echoArg);
        return echo(echoArg);
    }
    public void noop(ReferenceTest test, boolean optimized)
    {
        log.debug("StatelessSessionBean.noop");
    }

   public ReturnData getData()
   {
      ReturnData data = new ReturnData();
      data.data = "TheReturnData";
      return data;
   }
}
