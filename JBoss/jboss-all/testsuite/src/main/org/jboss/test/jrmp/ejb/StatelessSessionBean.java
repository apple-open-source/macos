package org.jboss.test.jrmp.ejb;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.jboss.test.jrmp.interfaces.IString;

/** A simple session bean for testing access over custom RMI sockets.

@author Scott_Stark@displayscape.com
@version $Revision: 1.4 $
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

    public String echo(String arg)
    {
        log.debug("StatelessSessionBean.echo, arg="+arg);
        return arg;
    }

    public IString copy(String arg)
    {
        log.debug("StatelessSessionBean.copy, arg="+arg);
        return new AString(arg);
    }
}
