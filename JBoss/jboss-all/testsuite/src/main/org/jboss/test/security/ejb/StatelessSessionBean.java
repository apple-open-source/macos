package org.jboss.test.security.ejb;

import java.rmi.RemoteException;
import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

/** A simple session bean for testing declarative security.

@author Scott.Stark@jboss.org
@version $Revision: 1.7 $
*/
public class StatelessSessionBean implements SessionBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
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
        if( isCaller == false )
            throw new SecurityException("Caller does not have EchoCaller role");
        return arg;
    }
    public String forward(String echoArg)
    {
        log.debug("forward, echoArg="+echoArg);
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
        log.debug("unchecked, callerPrincipal="+p);
    }

    public void excluded()
    {
        throw new EJBException("excluded, no access should be allowed");
    }

}
