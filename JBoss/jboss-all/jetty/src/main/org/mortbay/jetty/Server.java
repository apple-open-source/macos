// ========================================================================
// Copyright (c) 2002 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: Server.java,v 1.15.2.9 2003/06/04 04:47:49 starksm Exp $
// ========================================================================

package org.mortbay.jetty;

import java.io.IOException;
import java.lang.reflect.Method;
import java.net.URL;
import java.util.ArrayList;
import org.mortbay.http.HttpContext;
import org.mortbay.http.HttpServer;
import org.mortbay.jetty.servlet.ServletHttpContext;
import org.mortbay.jetty.servlet.WebApplicationContext;
import org.mortbay.util.Code;
import org.mortbay.util.Log;
import org.mortbay.util.Resource;
import org.mortbay.xml.XmlConfiguration;


/* ------------------------------------------------------------ */
/** The Jetty HttpServer.
 *
 * This specialization of org.mortbay.http.HttpServer adds knowledge
 * about servlets and their specialized contexts.   It also included
 * support for initialization from xml configuration files
 * that follow the XmlConfiguration dtd.
 *
 * HttpContexts created by Server are of the type
 * org.mortbay.jetty.servlet.ServletHttpContext unless otherwise
 * specified.
 *
 * This class also provides a main() method which starts a server for
 * each config file passed on the command line.  If the system
 * property JETTY_NO_SHUTDOWN_HOOK is not set to true, then a shutdown
 * hook is thread is registered to stop these servers.   
 *
 * @see org.mortbay.xml.XmlConfiguration
 * @see org.mortbay.jetty.servlet.ServletHttpContext
 * @version $Revision: 1.15.2.9 $
 * @author Greg Wilkins (gregw)
 */
public class Server extends HttpServer 
{
    private String _configuration;
    private String _rootWebApp;

    /* ------------------------------------------------------------ */
    /** Constructor. 
     */
    public Server()
    {}
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param configuration The filename or URL of the XML
     * configuration file.
     */
    public Server(String configuration)
        throws IOException
    {
        this(Resource.newResource(configuration).getURL());
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param configuration The filename or URL of the XML
     * configuration file.
     */
    public Server(Resource configuration)
        throws IOException
    {
        this(configuration.getURL());
    }
    
    /* ------------------------------------------------------------ */
    /** Constructor. 
     * @param configuration The filename or URL of the XML
     * configuration file.
     */
    public Server(URL configuration)
        throws IOException
    {
        _configuration=configuration.toString();
        try
        {
            XmlConfiguration config=new XmlConfiguration(configuration);
            config.configure(this);
        }
        catch(IOException e)
        {
            throw e;
        }
        catch(Exception e)
        {
            Code.warning(e);
            throw new IOException("Jetty configuration problem: "+e);
        }
    }

    /* ------------------------------------------------------------ */
    /** Get the root webapp name.
     * @return The name of the root webapp (eg. "root" for root.war). 
     */
    public String getRootWebApp()
    {
        return _rootWebApp;
    }
    
    /* ------------------------------------------------------------ */
    /** Set the root webapp name.
     * @param rootWebApp The name of the root webapp (eg. "root" for root.war). 
     */
    public void setRootWebApp(String rootWebApp)
    {
        _rootWebApp = rootWebApp;
    }
    
    /* ------------------------------------------------------------ */
    /**  Configure the server from an XML file.
     * @param configuration The filename or URL of the XML
     * configuration file.
     */
    public void configure(String configuration)
        throws IOException
    {

        URL url=Resource.newResource(configuration).getURL();
        if (_configuration!=null && _configuration.equals(url.toString()))
            return;
        if (_configuration!=null)
            throw new IllegalStateException("Already configured with "+_configuration);
        try
        {
            XmlConfiguration config=new XmlConfiguration(url);
            _configuration=url.toString();
            config.configure(this);
        }
        catch(IOException e)
        {
            throw e;
        }
        catch(Exception e)
        {
            Code.warning(e);
            throw new IOException("Jetty configuration problem: "+e);
        }
    }
    
    /* ------------------------------------------------------------ */
    public String getConfiguration()
    {
        return _configuration;
    }
    
    /* ------------------------------------------------------------ */
    /** Create a new ServletHttpContext.
     * Ths method is called by HttpServer to creat new contexts.  Thus
     * calls to addContext or getContext that result in a new Context
     * being created will return an
     * org.mortbay.jetty.servlet.ServletHttpContext instance.
     * @return ServletHttpContext
     */
    protected HttpContext newHttpContext()
    {
        return new ServletHttpContext();
    }
    
    /* ------------------------------------------------------------ */
    /** Create a new WebApplicationContext.
     * Ths method is called by Server to creat new contexts for web 
     * applications.  Thus calls to addWebApplication that result in 
     * a new Context being created will return an correct class instance.
     * Derived class can override this method to create instance of its
     * own class derived from WebApplicationContext in case it needs more
     * functionality.
     * @param webApp The Web application directory or WAR file.
     * @return WebApplicationContext
     */
    protected WebApplicationContext newWebApplicationContext(
       String webApp
    )
    {
        return new WebApplicationContext(webApp);
    }

    /* ------------------------------------------------------------ */
    /** Add Web Application.
     * @param contextPathSpec The context path spec. Which must be of
     * the form / or /path/*
     * @param webApp The Web application directory or WAR file.
     * @return The WebApplicationContext
     * @exception IOException 
     */
    public WebApplicationContext addWebApplication(String contextPathSpec,
                                                   String webApp)
        throws IOException
    {
        return addWebApplication(null,contextPathSpec,webApp);
    }
    
    /* ------------------------------------------------------------ */
    /** Add Web Application.
     * @param virtualHost Virtual host name or null
     * @param contextPathSpec The context path spec. Which must be of
     * the form / or /path/*
     * @param webApp The Web application directory or WAR file.
     * @return The WebApplicationContext
     * @exception IOException 
     */
    public WebApplicationContext addWebApplication(String virtualHost,
                                                   String contextPathSpec,
                                                   String webApp)
        throws IOException
    {
        WebApplicationContext appContext =
            newWebApplicationContext(webApp);
        appContext.setContextPath(contextPathSpec);
        addContext(virtualHost,appContext);
        Code.debug("Web Application ",appContext," added");
        return appContext;
    }

    
    /* ------------------------------------------------------------ */
    /**  Add Web Applications.
     * Add auto webapplications to the server.  The name of the
     * webapp directory or war is used as the context name. If a
     * webapp is called "root" it is added at "/".
     * @param webapps Directory file name or URL to look for auto webapplication.
     * @exception IOException 
     */
    public WebApplicationContext[] addWebApplications(String webapps)
        throws IOException
    {
        return addWebApplications(null,webapps,null,false);
    }
    
    /* ------------------------------------------------------------ */
    /**  Add Web Applications.
     * Add auto webapplications to the server.  The name of the
     * webapp directory or war is used as the context name. If the
     * webapp matches the rootWebApp it is added as the "/" context.
     * @param host Virtual host name or null
     * @param webapps Directory file name or URL to look for auto webapplication.
     * @exception IOException 
     */
    public WebApplicationContext[] addWebApplications(String host,
                                                      String webapps)
        throws IOException
    {
        return addWebApplications(host,webapps,null,false);
    }
        
    /* ------------------------------------------------------------ */
    /**  Add Web Applications.
     * Add auto webapplications to the server.  The name of the
     * webapp directory or war is used as the context name. If the
     * webapp matches the rootWebApp it is added as the "/" context.
     * @param host Virtual host name or null
     * @param webapps Directory file name or URL to look for auto
     * webapplication.
     * @param extract If true, extract war files
     * @exception IOException 
     */
    public WebApplicationContext[] addWebApplications(String host,
                                                      String webapps,
                                                      boolean extract)
        throws IOException
    {
        return addWebApplications(host,webapps,null,extract);
    }
    
    /* ------------------------------------------------------------ */
    /**  Add Web Applications.
     * Add auto webapplications to the server.  The name of the
     * webapp directory or war is used as the context name. If the
     * webapp matches the rootWebApp it is added as the "/" context.
     * @param host Virtual host name or null
     * @param webapps Directory file name or URL to look for auto
     * webapplication.
     * @param defaults The defaults xml filename or URL which is
     * loaded before any in the web app. Must respect the web.dtd.
     * If null the default defaults file is used. If the empty string, then
     * no defaults file is used.
     * @param extract If true, extract war files
     * @exception IOException 
     */
    public WebApplicationContext[] addWebApplications(String host,
                                                      String webapps,
                                                      String defaults,
                                                      boolean extract)
        throws IOException
    {
        ArrayList wacs = new ArrayList();
        Resource r=Resource.newResource(webapps);
        if (!r.exists())
            throw new IllegalArgumentException("No such webapps resource "+r);
        
        if (!r.isDirectory())
            throw new IllegalArgumentException("Not directory webapps resource "+r);
        
        String[] files=r.list();
        
        for (int f=0;files!=null && f<files.length;f++)
        {
            String context=files[f];
            
            if (context.equalsIgnoreCase("CVS/") ||
                context.equalsIgnoreCase("CVS") ||
                context.startsWith("."))
                continue;

            
            String app = r.addPath(r.encode(files[f])).toString();
            if (context.toLowerCase().endsWith(".war") ||
                context.toLowerCase().endsWith(".jar"))
            {
                context=context.substring(0,context.length()-4);
                Resource unpacked=r.addPath(context);
                if (unpacked!=null && unpacked.exists() && unpacked.isDirectory())
                    continue;
            }
            
            if (_rootWebApp!=null && (context.equals(_rootWebApp)||context.equals(_rootWebApp+"/")))
                context="/";
            else
                context="/"+context;

            WebApplicationContext wac= addWebApplication(host,
                                                         context,
                                                         app);
            wac.setExtractWAR(extract);
            if (defaults!=null)
            {
                if (defaults.length()==0)
                    wac.setDefaultsDescriptor(null);
                else
                    wac.setDefaultsDescriptor(defaults);
            }
            wacs.add(wac);
        }

        return (WebApplicationContext[])wacs.toArray(new WebApplicationContext[wacs.size()]);
    }

    
    /* ------------------------------------------------------------ */
    /** Add Web Application.
     * @param contextPathSpec The context path spec. Which must be of
     * the form / or /path/*
     * @param webApp The Web application directory or WAR as file or URL.
     * @param defaults The defaults xml filename or URL which is
     * loaded before any in the web app. Must respect the web.dtd.
     * Normally this is passed the file $JETTY_HOME/etc/webdefault.xml
     * @param extractWar If true, WAR files are extracted to the
     * webapp subdirectory of the contexts temporary directory.
     * @return The WebApplicationContext
     * @exception IOException 
     * @deprecated use addWebApplicaton(host,path,webapp)
     */
    public WebApplicationContext addWebApplication(String contextPathSpec,
                                                   String webApp,
                                                   String defaults,
                                                   boolean extractWar)
        throws IOException
    {
        return addWebApplication(null,
                                 contextPathSpec,
                                 webApp,
                                 defaults,
                                 extractWar);
    }
    
    
    /* ------------------------------------------------------------ */
    /**  Add Web Application.
     * @param virtualHost Virtual host name or null
     * @param contextPathSpec The context path spec. Which must be of
     * the form / or /path/*
     * @param webApp The Web application directory or WAR file.
     * @param defaults The defaults xml filename or URL which is
     * loaded before any in the web app. Must respect the web.dtd.
     * Normally this is passed the file $JETTY_HOME/etc/webdefault.xml
     * @param extractWar If true, WAR files are extracted to the
     * webapp subdirectory of the contexts temporary directory.
     * @return The WebApplicationContext
     * @exception IOException
     * @deprecated use addWebApplicaton(host,path,webapp)
     */
    public WebApplicationContext addWebApplication(String virtualHost,
                                                   String contextPathSpec,
                                                   String webApp,
                                                   String defaults,
                                                   boolean extractWar)
        throws IOException
    {
        Log.warning("DEPRECATED: use addWebApplicaton(host,path,webapp)");
        WebApplicationContext appContext =
            addWebApplication(virtualHost,contextPathSpec,webApp);
        appContext.setDefaultsDescriptor(defaults);
        appContext.setExtractWAR(extractWar);        
        return appContext;
    }

    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    public static void main(String[] arg)
    {
        String[] dftConfig={"etc/jetty.xml"};
        
        if (arg.length==0)
        {
            Log.event("Using default configuration: etc/jetty.xml");
            arg=dftConfig;
        }

        final Server[] servers=new Server[arg.length];

        // create and start the servers.
        for (int i=0;i<arg.length;i++)
        {
            try
            {
                servers[i] = new Server(arg[i]);
                servers[i].start();

            }
            catch(Exception e)
            {
                Code.warning(e);
            }
        }

        // Create and add a shutdown hook
        if (!Boolean.getBoolean("JETTY_NO_SHUTDOWN_HOOK"))
        {
            try
            {
                Method shutdownHook=
                    java.lang.Runtime.class
                    .getMethod("addShutdownHook",new Class[] {java.lang.Thread.class});
                Thread hook = 
                    new Thread() {
                            public void run()
                            {
                                setName("Shutdown");
                                Log.event("Shutdown hook executing");
                                for (int i=0;i<servers.length;i++)
                                {
				    if (servers[i]==null) continue;
                                    try{servers[i].stop();}
                                    catch(Exception e){Code.warning(e);}
                                }
                                
                                // Try to avoid JVM crash
                                try{Thread.sleep(1000);}
                                catch(Exception e){Code.warning(e);}
                            }
                        };
                shutdownHook.invoke(Runtime.getRuntime(),
                                    new Object[]{hook});
            }
            catch(Exception e)
            {
                Code.debug("No shutdown hook in JVM ",e);
            }
        }

        // create and start the servers.
        for (int i=0;i<arg.length;i++)
        {
            try{servers[i].join();}
            catch (Exception e){Code.ignore(e);}
        }
    }
}




