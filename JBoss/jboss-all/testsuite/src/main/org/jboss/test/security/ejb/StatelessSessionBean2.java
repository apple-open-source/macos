package org.jboss.test.security.ejb;

import java.rmi.RemoteException;
import java.security.Principal;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;

import org.jboss.test.security.interfaces.Entity;
import org.jboss.test.security.interfaces.EntityHome;
import org.jboss.test.security.interfaces.StatelessSession;
import org.jboss.test.security.interfaces.StatelessSessionHome;

/** A SessionBean that access the Entity bean to test Principal
identity propagation.

@author Scott.Stark@jboss.org
@version $Revision: 1.6 $
*/
public class StatelessSessionBean2 implements SessionBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    private SessionContext sessionContext;

    public void ejbCreate() throws RemoteException, CreateException
    {
        log.debug("ejbCreate() called");
    }

    public void ejbActivate() throws RemoteException
    {
        log.debug("ejbActivate() called");
    }

    public void ejbPassivate() throws RemoteException
    {
        log.debug("ejbPassivate() called");
    }

    public void ejbRemove() throws RemoteException
    {
        log.debug("ejbRemove() called");
    }

    public void setSessionContext(SessionContext context) throws RemoteException
    {
        sessionContext = context;
    }

    public String echo(String arg)
    {
        log.debug("echo, arg="+arg);
        // This call should fail if the bean is not secured
        Principal p = sessionContext.getCallerPrincipal();
        log.debug("echo, callerPrincipal="+p);
        String echo = null;
        try
        {
            InitialContext ctx = new InitialContext();
            EntityHome home = (EntityHome) ctx.lookup("java:comp/env/ejb/Entity");
            Entity bean = home.findByPrimaryKey(arg);
            echo = bean.echo(arg);
        }
        catch(Exception e)
        {
            log.debug("failed", e);
            e.fillInStackTrace();
            throw new EJBException(e);
        }
        return echo;
    }

    public String forward(String echoArg)
    {
        log.debug("forward, echoArg="+echoArg);
        String echo = null;
        try
        {
            InitialContext ctx = new InitialContext();
            StatelessSessionHome home = (StatelessSessionHome) ctx.lookup("java:comp/env/ejb/Session");
            StatelessSession bean = home.create();
            echo = bean.echo(echoArg);
        }
        catch(Exception e)
        {
            log.debug("failed", e);
            e.fillInStackTrace();
            throw new EJBException(e);
        }
        return echo;
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
        log.debug("StatelessSessionBean.unchecked, callerPrincipal="+p);
    }

    public void excluded()
    {
        throw new EJBException("StatelessSessionBean.excluded, no access should be allowed");
    }
}
