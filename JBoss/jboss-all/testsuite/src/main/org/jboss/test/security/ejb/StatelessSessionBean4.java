package org.jboss.test.security.ejb;

import java.rmi.RemoteException;
import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.apache.log4j.Category;

/** A simple session bean for testing declarative security and
the use of getCallerPrincipal in ejbCreate

@author Scott.Stark@jboss.org
@version $Revision: 1.4 $
*/
public class StatelessSessionBean4 implements SessionBean
{
    private static Category log = Category.getInstance(StatelessSessionBean4.class);

    private SessionContext sessionContext;

    public void ejbCreate() throws CreateException
    {
        log.debug("ejbCreate() called");
        Principal caller = sessionContext.getCallerPrincipal();
        log.debug("getCallerPrincipal() called");
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
        Principal p = sessionContext.getCallerPrincipal();
        log.debug("excluded, callerPrincipal="+p);
    }

}

