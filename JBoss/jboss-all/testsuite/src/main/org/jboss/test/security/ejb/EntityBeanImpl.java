package org.jboss.test.security.ejb;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.rmi.RemoteException;
import java.security.Principal;
import javax.ejb.EJBException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.security.auth.Subject;

/** A BMP entity bean that creates beans on the fly with
a key equal to that passed to findByPrimaryKey. Obviously
not a real entity bean. It is used to test Principal propagation
using the echo method. 

@author Scott.Stark@jboss.org
@version $Revision: 1.7 $
*/
public class EntityBeanImpl implements EntityBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    private String key;
    private EntityContext context;

    public void ejbActivate()
    {
        log.debug("EntityBean.ejbActivate() called");
    }

    public void ejbPassivate()
    {
        log.debug("EntityBean.ejbPassivate() called");
    }

    public void ejbRemove()
    {
        log.debug("EntityBean.ejbRemove() called");
    }
    public void ejbLoad()
    {
        log.debug("EntityBean.ejbLoad() called");
        key = (String) context.getPrimaryKey();
    }
    public void ejbStore()
    {
        log.debug("EntityBean.ejbStore() called");
    }

    public void setEntityContext(EntityContext context)
    {
        this.context = context;
    }
    public void unsetEntityContext()
    {
        this.context = null;
    }

    public String echo(String arg)
    {
        log.debug("EntityBean.echo, arg="+arg);
        Principal p = context.getCallerPrincipal();
        boolean isInternalRole = context.isCallerInRole("InternalRole");
        log.debug("EntityBean.echo, callerPrincipal="+p);
        log.debug("EntityBean.echo, isCallerInRole('InternalRole')="+isInternalRole);
        // Check the java:comp/env/security/security-domain
        try
        {
           InitialContext ctx = new InitialContext();
           Object securityMgr = ctx.lookup("java:comp/env/security/security-domain");
           log.debug("Checking java:comp/env/security/security-domain");
           if( securityMgr == null )
              throw new EJBException("Failed to find security mgr under: java:comp/env/security/security-domain");
           log.debug("Found SecurityManager: "+securityMgr);
           Subject activeSubject = (Subject) ctx.lookup("java:comp/env/security/subject");
           log.debug("ActiveSubject: "+activeSubject);
           if( activeSubject == null )
              throw new EJBException("No ActiveSubject found");
        }
        catch(NamingException e)
        {
           log.debug("failed", e);
           throw new EJBException("Naming exception: "+e.toString(true));
        }
        return p.getName();
    }

    public String ejbFindByPrimaryKey(String key)
    {
        log.debug("EntityBean.ejbFindByPrimaryKey, key="+key);
        return key;
    }
}
