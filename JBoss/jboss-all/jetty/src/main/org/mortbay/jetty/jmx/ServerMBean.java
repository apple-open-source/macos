// ========================================================================
// Copyright (c) 1999 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: ServerMBean.java,v 1.14.2.5 2003/06/04 04:47:50 starksm Exp $
// ========================================================================

package org.mortbay.jetty.jmx;

import java.io.IOException;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import org.mortbay.http.jmx.HttpServerMBean;
import org.mortbay.jetty.Server;
import org.mortbay.util.Code;

/* ------------------------------------------------------------ */
/** JettyServer MBean.
 * This Model MBean class provides the mapping for HttpServer
 * management methods. It also registers itself as a membership
 * listener of the HttpServer, so it can create and destroy MBean
 * wrappers for listeners and contexts.
 *
 * @version $Revision: 1.14.2.5 $
 * @author Greg Wilkins (gregw)
 */
public class ServerMBean extends HttpServerMBean
{
    private Server _jettyServer;
    private String _configuration;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     * @exception InstanceNotFoundException 
     */
    protected ServerMBean(Server jettyServer)
        throws MBeanException, InstanceNotFoundException
    {
        super(jettyServer);
    }

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @exception MBeanException 
     * @exception InstanceNotFoundException 
     */
    public ServerMBean()
        throws MBeanException, InstanceNotFoundException
    {
        this(new Server());
    }

    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param configuration URL or File to jetty.xml style configuration file
     * @exception IOException 
     * @exception MBeanException 
     * @exception InstanceNotFoundException 
     */
    public ServerMBean(String configuration)
        throws IOException,MBeanException, InstanceNotFoundException
    {
        this(new Server());
        _configuration=configuration;
    }

    /* ------------------------------------------------------------ */
    protected ObjectName newObjectName(MBeanServer server)
    {
        return uniqueObjectName(server, getDefaultDomain()+":Jetty=");
    }

    /* ------------------------------------------------------------ */
    protected void defineManagedResource()
    {
        super.defineManagedResource();
        
        defineAttribute("configuration");
        defineAttribute("rootWebApp");
        
        defineOperation("addWebApplication",
                        new String[]{"java.lang.String",
                                     "java.lang.String"},
                        IMPACT_ACTION);

        defineOperation("addWebApplication",
                        new String[]{"java.lang.String",
                                     "java.lang.String",
                                     "java.lang.String"},
                        IMPACT_ACTION);
        defineOperation("addWebApplications",
                        new String[]{"java.lang.String",
                                     "java.lang.String"},
                        IMPACT_ACTION);
        _jettyServer=(Server)getManagedResource();
    }
    
    
    
    /* ------------------------------------------------------------ */
    /** 
     * @param ok 
     */
    public void postRegister(Boolean ok)
    {
        super.postRegister(ok);
        
        if (ok.booleanValue())
        {
            if (_configuration!=null)
            {
                try
                {
                    _jettyServer.configure(_configuration);
                    _jettyServer.start();
                }
                catch(Exception e)
                {
                    Code.warning(e);
                }
            }
        }
    }
    
    /* ------------------------------------------------------------ */
    public void postDeregister()
    {
        _configuration=null;   
        super.postDeregister();
    }
}
