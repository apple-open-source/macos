// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: HttpContextMBean.java,v 1.14.2.10 2003/06/04 04:47:48 starksm Exp $
// ========================================================================

package org.mortbay.http.jmx;

import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import org.mortbay.http.HttpContext;
import org.mortbay.util.Code;
import org.mortbay.util.jmx.LifeCycleMBean;
import java.util.HashMap;


/* ------------------------------------------------------------ */
/**
 *
 * @version $Revision: 1.14.2.10 $
 * @author Greg Wilkins (gregw)
 */
public class HttpContextMBean extends LifeCycleMBean
{
    private HttpContext _httpContext;
    private HashMap _rlMap=new HashMap(3);

    /* ------------------------------------------------------------ */
    /** Constructor.
     * @exception MBeanException
     */
    public HttpContextMBean()
        throws MBeanException
    {}

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();

        defineAttribute("virtualHosts");
        defineAttribute("hosts");
        defineAttribute("contextPath");

        defineAttribute("handlers",READ_ONLY,ON_MBEAN);
        defineAttribute("requestLog",READ_ONLY,ON_MBEAN);
        
        defineAttribute("classPath");

        defineAttribute("realm");
        defineAttribute("realmName");

        defineAttribute("redirectNullPath");
        defineAttribute("resourceBase");
        defineAttribute("maxCachedFileSize");
        defineAttribute("maxCacheSize");
        defineOperation("flushCache",
                        IMPACT_ACTION);
        defineOperation("getResource",
                        new String[] {STRING},
                        IMPACT_ACTION);

        defineAttribute("welcomeFiles");
        defineOperation("addWelcomeFile",
                        new String[] {STRING},
                        IMPACT_INFO);
        defineOperation("removeWelcomeFile",
                        new String[] {STRING},
                        IMPACT_INFO);

        defineAttribute("mimeMap");
        defineOperation("setMimeMapping",new String[] {STRING,STRING},IMPACT_ACTION);

        
        defineAttribute("statsOn");
        defineAttribute("statsOnMs");
        defineOperation("statsReset",IMPACT_ACTION);
        defineAttribute("requests");
        defineAttribute("requestsActive");
        defineAttribute("requestsActiveMax");
        defineAttribute("responses1xx");
        defineAttribute("responses2xx");
        defineAttribute("responses3xx");
        defineAttribute("responses4xx");
        defineAttribute("responses5xx");

        defineOperation("stop",new String[] {"java.lang.Boolean.TYPE"},IMPACT_ACTION);

        defineOperation("destroy",
                        IMPACT_ACTION);

        defineOperation("setInitParameter",
                        new String[] {STRING,STRING},
                        IMPACT_ACTION);
        defineOperation("getInitParameter",
                        new String[] {STRING},
                        IMPACT_INFO);
        defineOperation("getInitParameterNames",
                        NO_PARAMS,
                        IMPACT_INFO);

        defineOperation("setAttribute",new String[] {STRING,OBJECT},IMPACT_ACTION);
        defineOperation("getAttribute",new String[] {STRING},IMPACT_INFO);
        defineOperation("getAttributeNames",NO_PARAMS,IMPACT_INFO);
        defineOperation("removeAttribute",new String[] {STRING},IMPACT_ACTION);

        defineOperation("addHandler",new String[] {"org.mortbay.http.HttpHandler"},IMPACT_ACTION);
        defineOperation("addHandler",new String[] {INT,"org.mortbay.http.HttpHandler"},IMPACT_ACTION);
        defineOperation("removeHandler",new String[] {INT},IMPACT_ACTION);


        _httpContext=(HttpContext)getManagedResource();
    }


    /* ------------------------------------------------------------ */
    protected ObjectName newObjectName(MBeanServer server)
    {
        ObjectName oName=super.newObjectName(server);
        String context=_httpContext.getContextPath();
        if (context.length()==0)
            context="/";
        try{oName=new ObjectName(oName+",context="+context);}
        catch(Exception e){Code.warning(e);}
        return oName;
    }

    /* ------------------------------------------------------------ */
    public void postRegister(Boolean ok)
    {
        super.postRegister(ok);
        if (ok.booleanValue())
            getHandlers();
    }

    /* ------------------------------------------------------------ */
    public void postDeregister()
    {
        _httpContext=null;
        super.postDeregister();
    }

    /* ------------------------------------------------------------ */
    public ObjectName[] getHandlers()
    {
        return getComponentMBeans(_httpContext.getHandlers(),null);
    }

    /* ------------------------------------------------------------ */
    public ObjectName getRequestLog()
    {
        Object o = _httpContext.getRequestLog();
        if (o==null)
            return null;
        
        ObjectName[] on=getComponentMBeans(new Object[]{o},_rlMap);
        if (on.length>0)
            return on[0];
        return null;
    }

}


