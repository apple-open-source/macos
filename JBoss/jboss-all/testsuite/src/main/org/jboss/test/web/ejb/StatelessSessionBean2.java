package org.jboss.test.web.ejb;

import java.security.Principal;
import javax.ejb.*;
import javax.naming.InitialContext;

import org.jboss.test.web.interfaces.ReferenceTest;
import org.jboss.test.web.interfaces.StatelessSession;
import org.jboss.test.web.interfaces.StatelessSessionHome;
import org.jboss.test.web.interfaces.ReturnData;

/** A stateless SessionBean 

@author  Scott.Stark@jboss.org
@version $Revision: 1.6.4.4 $
*/
public class StatelessSessionBean2 implements SessionBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    private SessionContext sessionContext;

    public void ejbCreate() throws CreateException
    {
        log.debug("StatelessSessionBean2.ejbCreate() called");
    }

    public void ejbActivate()
    {
        log.debug("StatelessSessionBean2.ejbActivate() called");
    }

    public void ejbPassivate()
    {
        log.debug("StatelessSessionBean2.ejbPassivate() called");
    }

    public void ejbRemove()
    {
        log.debug("StatelessSessionBean2.ejbRemove() called");
    }

    public void setSessionContext(SessionContext context)
    {
        sessionContext = context;
    }

    public String echo(String arg)
    {
        log.debug("StatelessSessionBean2.echo, arg="+arg);
        return arg;
    }

    public String forward(String echoArg)
    {
        log.debug("StatelessSessionBean2.forward, echoArg="+echoArg);
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

    public void noop(ReferenceTest test, boolean optimized)
    {
        boolean wasSerialized = test.getWasSerialized();
        log.debug("StatelessSessionBean2.noop, test.wasSerialized="+wasSerialized+", optimized="+optimized);
        if( optimized && wasSerialized == true )
            throw new EJBException("Optimized call had serialized argument");
        if( optimized == false && wasSerialized == false )
            throw new EJBException("NotOptimized call had non serialized argument");
    }

   public ReturnData getData()
   {
      ReturnData data = new ReturnData();
      data.data = "TheReturnData2";
      return data;
   }
}
