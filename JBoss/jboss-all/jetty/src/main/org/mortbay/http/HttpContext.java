// ========================================================================
// Copyright (c) 2000 Mort Bay Consulting (Australia) Pty. Ltd.
// $Id: HttpContext.java,v 1.17.2.21 2003/07/11 00:55:12 jules_gosnell Exp $
// ========================================================================

package org.mortbay.http;

import java.io.File;
import java.io.IOException;
import java.io.Serializable;
import java.net.MalformedURLException;
import java.net.Socket;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.Permission;
import java.security.PermissionCollection;
import java.security.Permissions;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.ResourceBundle;
import org.mortbay.http.SecurityConstraint.Authenticator;
import org.mortbay.util.CachedResource;
import org.mortbay.util.Code;
import org.mortbay.util.IO;
import org.mortbay.util.LifeCycle;
import org.mortbay.util.Log;
import org.mortbay.util.MultiException;
import org.mortbay.util.Resource;
import org.mortbay.util.StringUtil;
import org.mortbay.util.URI;


/* ------------------------------------------------------------ */
/** Context for a collection of HttpHandlers.
 * HTTP Context provides an ordered container for HttpHandlers
 * that share the same path prefix, filebase, resourcebase and/or
 * classpath.
 * <p>
 * A HttpContext is analagous to a ServletContext in the
 * Servlet API, except that it may contain other types of handler
 * other than servlets.
 * <p>
 * A ClassLoader is created for the context and it uses
 * Thread.currentThread().getContextClassLoader(); as it's parent loader.
 * The class loader is initialized during start(), when a derived
 * context calls initClassLoader() or on the first call to loadClass()
 * <p>
 *
 * <B>Note. that order is important when configuring a HttpContext.
 * For example, if resource serving is enabled before servlets, then resources
 * take priority.</B>
 *
 * @see HttpServer
 * @see HttpHandler
 * @see org.mortbay.jetty.servlet.ServletHttpContext
 * @version $Id: HttpContext.java,v 1.17.2.21 2003/07/11 00:55:12 jules_gosnell Exp $
 * @author Greg Wilkins (gregw)
 */
public class HttpContext implements LifeCycle,
                                    Serializable
{
    /* ------------------------------------------------------------ */
    /** File class path attribute.
     * If this name is set as a context init parameter, then the attribute
     * name given will be used to set the file classpath for the context as a
     * context attribute.
     */
    public final static String __fileClassPathAttr=
        "org.mortbay.http.HttpContext.FileClassPathAttribute";

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private final static Map __dftMimeMap = new HashMap();
    private final static Map __encodings = new HashMap();
    static
    {
        ResourceBundle mime = ResourceBundle.getBundle("org/mortbay/http/mime");
        Enumeration i = mime.getKeys();
        while(i.hasMoreElements())
        {
            String ext = (String)i.nextElement();
            __dftMimeMap.put(ext,mime.getString(ext));
        }
        ResourceBundle encoding = ResourceBundle.getBundle("org/mortbay/http/encoding");
        i = encoding.getKeys();
        while(i.hasMoreElements())
        {
            String type = (String)i.nextElement();
            __encodings.put(type,encoding.getString(type));
        }
    }

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    // These attributes are serialized by WebApplicationContext, which needs
    // to be updated if you add to these
    private String _contextPath;
    private List _vhosts=new ArrayList(2);
    private List _hosts=new ArrayList(2);
    private List _handlers=new ArrayList(3);
    private Map _attributes = new HashMap(3);
    private boolean _redirectNullPath=false;
    private int _maxCachedFileSize =100*1024;
    private int _maxCacheSize =1024*1024;
    private boolean _statsOn=false;
    private PermissionCollection _permissions;
    private boolean _classLoaderJava2Compliant;

    /* ------------------------------------------------------------ */
    private String _contextName;
    private String _classPath;
    private Map _initParams = new HashMap(11);
    private Map _errorPages;
    private UserRealm _userRealm;
    private String _realmName;
    private PathMap _constraintMap=new PathMap();
    private Authenticator _authenticator;
    private RequestLog _requestLog;

    private Resource _resourceBase;
    private Map _mimeMap;
    private Map _encodingMap;

    private String[] _welcomes=
    {
        "welcome.html",
        "index.html",
        "index.htm",
        "index.jsp"
    };


    /* ------------------------------------------------------------ */
    private transient boolean _started;
    private transient ClassLoader _parent;
    private transient ClassLoader _loader;
    private transient HttpServer _httpServer;
    private transient File _tmpDir;
    private transient Map _cache=new HashMap();
    private transient int _cacheSize;
    private transient CachedMetaData _mostRecentlyUsed;
    private transient CachedMetaData _leastRecentlyUsed;
    private transient HttpHandler[] _handlersArray;
    private transient String[] _vhostsArray;

    /* ------------------------------------------------------------ */
    transient Object _statsLock=new Object[0];
    transient long _statsStartedAt;
    transient int _requests;
    transient int _requestsActive;
    transient int _requestsActiveMax;
    transient int _responses1xx; // Informal
    transient int _responses2xx; // Success
    transient int _responses3xx; // Redirection
    transient int _responses4xx; // Client Error
    transient int _responses5xx; // Server Error


    /* ------------------------------------------------------------ */
    /** Constructor.
     */
    public HttpContext()
    {}

    /* ------------------------------------------------------------ */
    /** Constructor.
     * @param httpServer
     * @param contextPathSpec
     */
    public HttpContext(HttpServer httpServer,String contextPathSpec)
    {
        this();
        setHttpServer(httpServer);
        setContextPath(contextPathSpec);
    }

    /* ------------------------------------------------------------ */
    private void readObject(java.io.ObjectInputStream in)
        throws IOException, ClassNotFoundException
    {
        in.defaultReadObject();
        _statsLock=new Object[0];
        _cache=new HashMap();
        getHandlers();
        for (int i=0;i<_handlersArray.length;i++)
            _handlersArray[i].initialize(this);
    }

    /* ------------------------------------------------------------ */
    /** Get the ThreadLocal HttpConnection.
     * Get the HttpConnection for current thread, if any.  This method is
     * not static in order to control access.
     * @return HttpConnection for this thread.
     */
    public HttpConnection getHttpConnection()
    {
        return HttpConnection.getHttpConnection();
    }

    /* ------------------------------------------------------------ */
    void setHttpServer(HttpServer httpServer)
    {
        _httpServer=httpServer;
        _contextName=null;
    }

    /* ------------------------------------------------------------ */
    public HttpServer getHttpServer()
    {
        return _httpServer;
    }

    /* ------------------------------------------------------------ */
    public static String canonicalContextPathSpec(String contextPathSpec)
    {
        // check context path
        if (contextPathSpec==null ||
            contextPathSpec.indexOf(',')>=0 ||
            contextPathSpec.startsWith("*"))
            throw new IllegalArgumentException ("Illegal context spec:"+contextPathSpec);

        if(!contextPathSpec.startsWith("/"))
	    contextPathSpec='/'+contextPathSpec;

        if (contextPathSpec.length()>1)
        {
            if (contextPathSpec.endsWith("/"))
                contextPathSpec+="*";
            else if (!contextPathSpec.endsWith("/*"))
                contextPathSpec+="/*";
        }

        return contextPathSpec;
    }

    /* ------------------------------------------------------------ */
    public void setContextPath(String contextPathSpec)
    {
        if (_httpServer!=null)
            _httpServer.removeMappings(this);

        contextPathSpec=canonicalContextPathSpec(contextPathSpec);

        if (contextPathSpec.length()>1)
            _contextPath=contextPathSpec.substring(0,contextPathSpec.length()-2);
        else
            _contextPath="/";

        _contextName=null;

        if (_httpServer!=null)
            _httpServer.addMappings(this);
    }


    /* ------------------------------------------------------------ */
    /**
     * @return The context prefix
     */
    public String getContextPath()
    {
        return _contextPath;
    }


    /* ------------------------------------------------------------ */
    /** Add a virtual host alias to this context.
     * @see #setVirtualHosts
     * @param hostname A hostname. A null host name means any hostname is
     * acceptable. Host names may String representation of IP addresses.
     */
    public void addVirtualHost(String hostname)
    {
        // Note that null hosts are also added.
        if (!_vhosts.contains(hostname))
        {
            _vhosts.add(hostname);
            _contextName=null;

            if (_httpServer!=null)
            {
                if (_vhosts.size()==1)
                    _httpServer.removeMapping(null,this);
                _httpServer.addMapping(hostname,this);
            }
            _vhostsArray=null;
        }
    }

    /* ------------------------------------------------------------ */
    /** remove a virtual host alias to this context.
     * @see #setVirtualHosts
     * @param hostname A hostname. A null host name means any hostname is
     * acceptable. Host names may String representation of IP addresses.
     */
    public void removeVirtualHost(String hostname)
    {
        // Note that null hosts are also added.
        if (_vhosts.remove(hostname))
        {
            _contextName=null;
            if (_httpServer!=null)
            {
                _httpServer.removeMapping(hostname,this);
                if (_vhosts.size()==0)
                    _httpServer.addMapping(null,this);
            }
            _vhostsArray=null;
        }
    }

    /* ------------------------------------------------------------ */
    /** Set the virtual hosts for the context.
     * Only requests that have a matching host header or fully qualified
     * URL will be passed to that context with a virtual host name.
     * A context with no virtual host names or a null virtual host name is
     * available to all requests that are not served by a context with a
     * matching virtual host name.
     * @param hosts Array of virtual hosts that this context responds to. A
     * null host name or null/empty array means any hostname is acceptable.
     * Host names may String representation of IP addresses.
     */
    public void setVirtualHosts(String[] hosts)
    {
        List old = new ArrayList(_vhosts);

        for (int i=0;i<hosts.length;i++)
        {
            boolean existing=old.remove(hosts[i]);
            if (!existing)
                addVirtualHost(hosts[i]);
        }

        for (int i=0;i<old.size();i++)
            removeVirtualHost((String)old.get(i));
    }

    /* ------------------------------------------------------------ */
    /** Set the hosts for the context.
     * Set the real hosts that this context will accept requests for.
     * If not null or empty, then only requests from HttpListeners for hosts
     * in this array are accepted by this context. 
     * Unlike virutal hosts, this value is not used by HttpServer for
     * matching a request to a context.
     */
    public void setHosts(String[] hosts)
        throws UnknownHostException
    {
        if (hosts==null || hosts.length==0)
            _hosts=null;
        else
        {
            _hosts=new ArrayList();
            for (int i=0;i<hosts.length;i++)
                if (hosts[i]!=null)
                    _hosts.add(InetAddress.getByName(hosts[i]));
        }
        
    }

    /* ------------------------------------------------------------ */
    /** Get the hosts for the context.
     */
    public String[] getHosts()
    {
        if (_hosts==null || _hosts.size()==0)
            return null;
        String[] hosts=new String[_hosts.size()];
        for (int i=0;i<hosts.length;i++)
        {
            InetAddress a = (InetAddress)_hosts.get(i);
            if (a!=null)
                hosts[i]=a.getHostName();
        }
        return hosts;
    }


    /* ------------------------------------------------------------ */
    /** Get the virtual hosts for the context.
     * Only requests that have a matching host header or fully qualified
     * URL will be passed to that context with a virtual host name.
     * A context with no virtual host names or a null virtual host name is
     * available to all requests that are not served by a context with a
     * matching virtual host name.
     * @return Array of virtual hosts that this context responds to. A
     * null host name or empty array means any hostname is acceptable.
     * Host names may be String representation of IP addresses.
     */
    public String[] getVirtualHosts()
    {
        if (_vhostsArray!=null)
            return _vhostsArray;
        if (_vhosts==null)
            _vhostsArray=new String[0];
        else
        {
            _vhostsArray=new String[_vhosts.size()];
            _vhostsArray=(String[])_vhosts.toArray(_vhostsArray);
        }
        return _vhostsArray;
    }


    /* ------------------------------------------------------------ */
    public void setHandlers(HttpHandler[] handlers)
    {
        List old = new ArrayList(_handlers);

        for (int i=0;i<handlers.length;i++)
        {
            boolean existing=old.remove(handlers[i]);
            if (!existing)
                addHandler(handlers[i]);
        }

        for (int i=0;i<old.size();i++)
            removeHandler((HttpHandler)old.get(i));
    }

    /* ------------------------------------------------------------ */
    /** Get all handlers.
     * @return List of all HttpHandlers
     */
    public HttpHandler[] getHandlers()
    {
        if (_handlersArray!=null)
            return _handlersArray;
        if (_handlers==null)
            _handlersArray=new HttpHandler[0];
        else
        {
            _handlersArray=new HttpHandler[_handlers.size()];
            _handlersArray=(HttpHandler[])_handlers.toArray(_handlersArray);
        }
        return _handlersArray;
    }


    /* ------------------------------------------------------------ */
    /** Add a handler.
     * @param i The position in the handler list
     * @param handler The handler.
     */
    public synchronized void addHandler(int i,HttpHandler handler)
    {
        _handlers.add(i,handler);
        _handlersArray=null;

        HttpContext context = handler.getHttpContext();
        if (context==null)
            handler.initialize(this);
        else if (context!=this)
            throw new IllegalArgumentException("Handler already initialized in another HttpContext");
    }

    /* ------------------------------------------------------------ */
    /** Add a HttpHandler to the context.
     * @param handler
     */
    public synchronized void addHandler(HttpHandler handler)
    {
        addHandler(_handlers.size(),handler);
    }

    /* ------------------------------------------------------------ */
    /** Get handler index.
     * @param handler instance
     * @return Index of handler in context or -1 if not found.
     */
    public int getHandlerIndex(HttpHandler handler)
    {
        for (int h=0;h<_handlers.size();h++)
        {
            if ( handler == _handlers.get(h))
                return h;
        }
        return -1;
    }

    /* ------------------------------------------------------------ */
    /** Get a handler by class.
     * @param handlerClass
     * @return The first handler that is an instance of the handlerClass
     */
    public synchronized HttpHandler getHandler(Class handlerClass)
    {
        for (int h=0;h<_handlers.size();h++)
        {
            HttpHandler handler = (HttpHandler)_handlers.get(h);
            if (handlerClass.isInstance(handler))
                return handler;
        }
        return null;
    }

    /* ------------------------------------------------------------ */
    /** Remove a handler.
     * The handler must be stopped before being removed.
     * @param i index of handler
     */
    public synchronized HttpHandler removeHandler(int i)
    {
        HttpHandler handler = _handlersArray[i];
        if (handler.isStarted())
            try{handler.stop();} catch (InterruptedException e){Code.warning(e);}
        _handlers.remove(i);
        _handlersArray=null;
        return handler;
    }

    /* ------------------------------------------------------------ */
    /** Remove a handler.
     * The handler must be stopped before being removed.
     */
    public synchronized void removeHandler(HttpHandler handler)
    {
        if (handler.isStarted())
            try{handler.stop();} catch (InterruptedException e){Code.warning(e);}
        _handlers.remove(handler);
        _handlersArray=null;
    }


    /* ------------------------------------------------------------ */
    /** Set context init parameter.
     * Init Parameters differ from attributes as they can only
     * have string values, servlets cannot set them and they do
     * not have a package scoped name space.
     * @param param param name
     * @param value param value or null
     */
    public void setInitParameter(String param, String value)
    {
        _initParams.put(param,value);
    }

    /* ------------------------------------------------------------ */
    /** Get context init parameter.
     * @param param param name
     * @return param value or null
     */
    public String getInitParameter(String param)
    {
        return (String)_initParams.get(param);
    }

    /* ------------------------------------------------------------ */
    /** Get context init parameter.
     * @return Enumeration of names
     */
    public Enumeration getInitParameterNames()
    {
        return Collections.enumeration(_initParams.keySet());
    }

    /* ------------------------------------------------------------ */
    /** Set a context attribute.
     * Attributes are cleared when the context is stopped.
     * @param name attribute name
     * @param value attribute value
     */
    public synchronized void setAttribute(String name, Object value)
    {
        _attributes.put(name,value);
    }

    /* ------------------------------------------------------------ */
    /**
     * @param name attribute name
     * @return attribute value or null
     */
    public Object getAttribute(String name)
    {
        return _attributes.get(name);
    }

    /* ------------------------------------------------------------ */
    /**
     */
    public Map getAttributes()
    {
        return _attributes;
    }

    /* ------------------------------------------------------------ */
    /**
     */
    public void setAttributes(Map attributes)
    {
        _attributes=attributes;
    }

    /* ------------------------------------------------------------ */
    /**
     * @return enumaration of names.
     */
    public Enumeration getAttributeNames()
    {
        return Collections.enumeration(_attributes.keySet());
    }

    /* ------------------------------------------------------------ */
    /**
     * @param name attribute name
     */
    public synchronized void removeAttribute(String name)
    {
        _attributes.remove(name);
    }



    /* ------------------------------------------------------------ */
    /** Set the Resource Base.
     * The base resource is the Resource to use as a relative base
     * for all context resources. The ResourceBase attribute is a
     * string version of the baseResource.
     * If a relative file is passed, it is converted to a file
     * URL based on the current working directory.
     * @return The file or URL to use as the base for all resources
     * within the context.
     */
    public String getResourceBase()
    {
        if (_resourceBase==null)
            return null;
        return _resourceBase.toString();
    }

    /* ------------------------------------------------------------ */
    /** Set the Resource Base.
     * The base resource is the Resource to use as a relative base
     * for all context resources. The ResourceBase attribute is a
     * string version of the baseResource.
     * If a relative file is passed, it is converted to a file
     * URL based on the current working directory.
     * @param resourceBase A URL prefix or directory name.
     */
    public void setResourceBase(String resourceBase)
    {
        try{
            _resourceBase=Resource.newResource(resourceBase);
            Code.debug("resourceBase=",_resourceBase," for ", this);
        }
        catch(IOException e)
        {
            Code.debug(e);
            throw new IllegalArgumentException(resourceBase+":"+e.toString());
        }
    }


    /* ------------------------------------------------------------ */
    /** Get the base resource.
     * The base resource is the Resource to use as a relative base
     * for all context resources. The ResourceBase attribute is a
     * string version of the baseResource.
     * @return The resourceBase as a Resource instance
     */
    public Resource getBaseResource()
    {
        return _resourceBase;
    }

    /* ------------------------------------------------------------ */
    /** Set the base resource.
     * The base resource is the Resource to use as a relative base
     * for all context resources. The ResourceBase attribute is a
     * string version of the baseResource.
     * @param base The resourceBase as a Resource instance
     */
    public void setBaseResource(Resource base)
    {
        _resourceBase=base;
    }


    /* ------------------------------------------------------------ */
    public int getMaxCachedFileSize()
    {
        return _maxCachedFileSize;
    }

    /* ------------------------------------------------------------ */
    public void setMaxCachedFileSize(int maxCachedFileSize)
    {
        _maxCachedFileSize = maxCachedFileSize;
        _cache.clear();
    }

    /* ------------------------------------------------------------ */
    public int getMaxCacheSize()
    {
        return _maxCacheSize;
    }

    /* ------------------------------------------------------------ */
    public void setMaxCacheSize(int maxCacheSize)
    {
        _maxCacheSize = maxCacheSize;
        _cache.clear();
    }

    /* ------------------------------------------------------------ */
    public void flushCache()
    {
        _cache.clear();
        System.gc();
    }

    /* ------------------------------------------------------------ */
    public String[] getWelcomeFiles()
    {
        return _welcomes;
    }

    /* ------------------------------------------------------------ */
    public void setWelcomeFiles(String[] welcomes)
    {
        if (welcomes==null)
            _welcomes=new String[0];
        else
            _welcomes=welcomes;
    }

    /* ------------------------------------------------------------ */
    public void addWelcomeFile(String welcomeFile)
    {
        if (welcomeFile.startsWith("/") ||
            welcomeFile.startsWith(java.io.File.separator) ||
            welcomeFile.endsWith("/") ||
            welcomeFile.endsWith(java.io.File.separator))
            Code.warning("Invalid welcome file: "+welcomeFile);
        List list = new ArrayList(Arrays.asList(_welcomes));
        list.add(welcomeFile);
        _welcomes=(String[])list.toArray(_welcomes);
    }

    /* ------------------------------------------------------------ */
    public void removeWelcomeFile(String welcomeFile)
    {
        List list = new ArrayList(Arrays.asList(_welcomes));
        list.remove(welcomeFile);
        _welcomes=(String[])list.toArray(_welcomes);
    }

    /* ------------------------------------------------------------ */
    /** Get a resource from the context.
     * Cached Resources are returned if the resource fits within the LRU
     * cache.  Directories may have CachedResources returned, but the
     * caller must use the CachedResource.setCachedData method to set the
     * formatted directory content.
     *
     * @param pathInContext
     * @return Resource
     * @exception IOException
     */
    public Resource getResource(String pathInContext)
        throws IOException
    {
        if (_resourceBase==null)
            return null;

        Resource resource=null;

        // Cache operations
        synchronized(_cache)
        {
            // Look for it in the cache
            CachedResource cached = (CachedResource)_cache.get(pathInContext);
            if (cached!=null)
            {
                if (Code.verbose()) Code.debug("CACHE HIT: ",cached);
                CachedMetaData cmd = (CachedMetaData)cached.getAssociate();
                if (cmd!=null && cmd.isValid())
                    return cached;
            }

            // Make the resource
            resource=_resourceBase.addPath(_resourceBase.encode(pathInContext));
            if (Code.verbose()) Code.debug("CACHE MISS: ",resource);
            if (resource==null)
                return null;

            
            // Check for file aliasing
            if (resource.getAlias()!=null)
            {
                Code.warning("Alias request of '"+resource.getAlias()+
                             "' for '"+resource+"'");
                return null;
            }

            // Is it an existing file?
            long len = resource.length();
            if (resource.exists())
            {
                // Is it badly named?
                if (!resource.isDirectory() && pathInContext.endsWith("/"))
                    return null;

                // Guess directory length.
                if (resource.isDirectory())
                {
                    if (resource.list()!=null)
                        len=resource.list().length*100;
                    else
                        len=0;
                }

                // Is it cacheable?
                if (len>0 && len<_maxCachedFileSize && len<_maxCacheSize)
                {
                    int needed=_maxCacheSize-(int)len;
                    while(_cacheSize>needed)
                        _leastRecentlyUsed.invalidate();

                    cached=resource.cache();
                    if (Code.verbose()) Code.debug("CACHED: ",resource);
                    new CachedMetaData(cached,pathInContext);
                    return cached;
                }
            }
        }

        // Non cached response
        new ResourceMetaData(resource);
        return resource;
    }

    /* ------------------------------------------------------------ */
    public String getWelcomeFile(Resource resource)
        throws IOException
    {
        if (!resource.isDirectory())
            return null;

        for (int i=0;i<_welcomes.length;i++)
        {
            Resource welcome=resource.addPath(_welcomes[i]);
            if (welcome.exists())
                return _welcomes[i];
        }

        return null;
    }


    /* ------------------------------------------------------------ */
    public synchronized Map getMimeMap()
    {
        return _mimeMap;
    }

    /* ------------------------------------------------------------ */
    /**
     * Also sets the org.mortbay.http.mimeMap context attribute
     * @param mimeMap
     */
    public void setMimeMap(Map mimeMap)
    {
        _mimeMap = mimeMap;
    }

    /* ------------------------------------------------------------ */
    /** Get the MIME type by filename extension.
     * @param filename A file name
     * @return MIME type matching the longest dot extension of the
     * file name.
     */
    public String getMimeByExtension(String filename)
    {
        String type=null;

        if (filename!=null)
        {
            int i=-1;
            while(type==null)
            {
                i=filename.indexOf(".",i+1);

                if (i<0 || i>=filename.length())
                    break;

                String ext=StringUtil.asciiToLowerCase(filename.substring(i+1));
                if (_mimeMap!=null)
                    type = (String)_mimeMap.get(ext);
                if (type==null)
                    type=(String)__dftMimeMap.get(ext);
            }
        }

        if (type==null)
        {
            if (_mimeMap!=null)
                type=(String)_mimeMap.get("*");
             if (type==null)
                 type=(String)__dftMimeMap.get("*");
        }

        return type;
    }

    /* ------------------------------------------------------------ */
    /** Set a mime mapping
     * @param extension
     * @param type
     */
    public void setMimeMapping(String extension,String type)
    {
        if (_mimeMap==null)
            _mimeMap=new HashMap();
        _mimeMap.put(extension,type);
    }


    /* ------------------------------------------------------------ */
    /** Get the context classpath.
     * This method only returns the paths that have been set for this
     * context and does not include any paths from a parent or the
     * system classloader.
     * Note that this may not be a legal javac classpath.
     * @return a comma or ';' separated list of class
     * resources. These may be jar files, directories or URLs to jars
     * or directories.
     * @see #getFileClassPath()
     */
    public String getClassPath()
    {
        return _classPath;
    }

    /* ------------------------------------------------------------ */
    /** Get the file classpath of the context.
     * This method makes a best effort to return a complete file
     * classpath for the context.  The default implementation returns
     * <PRE>
     *  ((ContextLoader)getClassLoader()).getFileClassPath()+
     *       System.getProperty("path.separator")+
     *       System.getProperty("java.class.path");
     * </PRE>
     * The default implementation requires the classloader to be
     * initialized before it is called. It will not include any
     * classpaths used by a non-system parent classloader.
     * <P>
     * The main user of this method is the start() method.  If a JSP
     * servlet is detected, the string returned from this method is
     * used as the default value for the "classpath" init parameter.
     * <P>
     * Derivations may replace this method with a more accurate or
     * specialized version.
     * @return Path of files and directories for loading classes.
     * @exception IllegalStateException HttpContext.initClassLoader
     * has not been called.
     */
    public String getFileClassPath()
        throws IllegalStateException
    {
        ClassLoader loader = getClassLoader();
        if (loader==null)
            throw new IllegalStateException("Context classloader not initialized");
        String fileClassPath =
            ((loader instanceof ContextLoader)
             ? ((ContextLoader)loader).getFileClassPath()
             : getClassPath())+
            System.getProperty("path.separator")+
            System.getProperty("java.class.path");
        return fileClassPath;
    }

    /* ------------------------------------------------------------ */
    /** Sets the class path for the context.
     * A class path is only required for a context if it uses classes
     * that are not in the system class path.
     * @param classPath a comma or ';' separated list of class
     * resources. These may be jar files, directories or URLs to jars
     * or directories.
     */
    public void setClassPath(String classPath)
    {
        _classPath=classPath;
        if (isStarted())
            Code.warning("classpath set while started");
    }

    /* ------------------------------------------------------------ */
    /** Sets the class path for the context from the jar and zip files found
     *  in the specified resource.
     * @param lib the resource that contains the jar and/or zip files.
     * @param append true if the classpath entries are to be appended to any
     * existing classpath, or false if they replace the existing classpath.
     * @see #setClassPath(String)
     */
    public void setClassPaths(Resource lib, boolean append)
    {
        if (isStarted())
            Code.warning("classpaths set while started");

        if (lib.exists() && lib.isDirectory())
        {
            StringBuffer classPath=new StringBuffer();

            if (append && this.getClassPath()!=null)
                classPath.append(_classPath);

            String[] files=lib.list();
            for (int f=0;files!=null && f<files.length;f++)
            {
                try {
                    Resource fn=lib.addPath(files[f]);
                    String fnlc=fn.getName().toLowerCase();
                    if (fnlc.endsWith(".jar") || fnlc.endsWith(".zip"))
                    {
                        classPath.append(classPath.length()>0?",":"");
                        classPath.append(fn.toString());
                    }
                }
                catch (Exception ex)
                {
                    Code.warning(ex);
                }
            }

            if (classPath.length()>0)
                _classPath=classPath.toString();
        }
    }

    /* ------------------------------------------------------------ */
    /** Sets the class path for the context from the jar and zip files found
     *  in the specified resource.
     * @param lib the resource that contains the jar and/or zip files.
     * @param append true if the classpath entries are to be appended to any
     * existing classpath, or false if they are to be prepended.
     * @exception IOException
     */
    public void setClassPaths(String lib, boolean append) throws IOException
    {
        if (_loader!=null)
            throw new IllegalStateException("ClassLoader already initialized");
        this.setClassPaths(Resource.newResource(lib), append);
    }

    /* ------------------------------------------------------------ */
    /** Get Java2 compliant classloading.
     * @return If true, the class loader will conform to the java 2
     * specification and delegate all loads to the parent classloader. If
     * false, the context classloader only delegate loads for system classes
     * or classes that it can't find itself.
     */
    public boolean isClassLoaderJava2Compliant()
    {
        return _classLoaderJava2Compliant;
    }

    /* ------------------------------------------------------------ */
    /** Set Java2 compliant classloading.
     * @param compliant If true, the class loader will conform to the java 2
     * specification and delegate all loads to the parent classloader. If
     * false, the context classloader only delegate loads for system classes
     * or classes that it can't find itself.
     */
    public void setClassLoaderJava2Compliant(boolean compliant)
    {
        _classLoaderJava2Compliant = compliant;
        if (_loader!=null && (_loader instanceof ContextLoader))
            ((ContextLoader)_loader).setJava2Compliant(compliant);
    }

    /* ------------------------------------------------------------ */
    /** Set temporary directory for context.
     * The javax.servlet.context.tempdir attribute is also set.
     * @param dir Writable temporary directory.
     */
    public void setTempDirectory(File dir)
    {
        if (isStarted())
            throw new IllegalStateException("Started");

        if (dir!=null)
        {
            try{dir=new File(dir.getCanonicalPath());}
            catch (IOException e){Code.warning(e);}
        }

        if (dir!=null && !dir.exists())
        {
            dir.mkdir();
            dir.deleteOnExit();
        }

        if (dir!=null && ( !dir.exists() || !dir.isDirectory() || !dir.canWrite()))
            throw new IllegalArgumentException("Bad temp directory: "+dir);

        _tmpDir=dir;
        setAttribute("javax.servlet.context.tempdir",_tmpDir);
    }

    /* ------------------------------------------------------------ */
    /** Get Context temporary directory.
     * A tempory directory is generated if it has not been set.  The
     * "javax.servlet.context.tempdir" attribute is consulted and if
     * not set, the host, port and context are used to generate a
     * directory within the JVMs temporary directory.
     * @return Temporary directory as a File.
     */
    public File getTempDirectory()
    {
        if (_tmpDir!=null)
            return _tmpDir;

        // Initialize temporary directory
        //
        // I'm afraid that this is very much black magic.
        // but if you can think of better....
        Object t = getAttribute("javax.servlet.context.tempdir");

        if (t!=null && (t instanceof File))
        {
            _tmpDir=(File)t;
            if (_tmpDir.isDirectory() && _tmpDir.canWrite())
                return _tmpDir;
        }

        if (t!=null && (t instanceof String))
        {
            try
            {
                _tmpDir=new File((String)t);

                if (_tmpDir.isDirectory() && _tmpDir.canWrite())
                {
                    Code.debug("Converted to File ",_tmpDir," for ",this);
                    setAttribute("javax.servlet.context.tempdir",_tmpDir);
                    return _tmpDir;
                }
            }
            catch(Exception e)
            {
                Code.warning(e);
            }
        }

        // No tempdir set so make one!
        try
        {
            HttpListener httpListener=_httpServer.getListeners()[0];

            String vhost = null;
            for (int h=0;vhost==null && _vhosts!=null && h<_vhosts.size();h++)
                vhost=(String)_vhosts.get(h);
            String host=httpListener.getHost();
            String temp="Jetty_"+
                (host==null?"":host)+
                "_"+
                httpListener.getPort()+
                "_"+
                (vhost==null?"":vhost)+
                getContextPath();

            temp=temp.replace('/','_');
            temp=temp.replace('.','_');
            temp=temp.replace('\\','_');

            _tmpDir=new File(System.getProperty("java.io.tmpdir"),temp);
            if (_tmpDir.exists())
            {
                Code.debug("Delete existing temp dir ",_tmpDir," for ",this);
                if (!IO.delete(_tmpDir))
                    Code.debug("Failed to delete temp dir "+_tmpDir);

                if (_tmpDir.exists())
                {
                    String old=_tmpDir.toString();
                    _tmpDir=File.createTempFile(temp+"_","");
                    if (_tmpDir.exists())
                        _tmpDir.delete();
                    Code.warning("Can't reuse "+old+", using "+_tmpDir);
                }
            }

            _tmpDir.mkdir();
            _tmpDir.deleteOnExit();
            Code.debug("Created temp dir ",_tmpDir," for ",this);
        }
        catch(Exception e)
        {
            _tmpDir=null;
            Code.ignore(e);
        }

        if (_tmpDir==null)
        {
            try{
                // that didn't work, so try something simpler (ish)
                _tmpDir=File.createTempFile("JettyContext","");
                if (_tmpDir.exists())
                    _tmpDir.delete();
                _tmpDir.mkdir();
                _tmpDir.deleteOnExit();
                Code.debug("Created temp dir ",_tmpDir," for ",this);
            }
            catch(IOException e)
            {
                Code.fail(e);
            }
        }

        setAttribute("javax.servlet.context.tempdir",_tmpDir);
        return _tmpDir;
    }



    /* ------------------------------------------------------------ */
    /** Set ClassLoader.
     * @param loader The loader to be used by this context.
     */
    public synchronized void setClassLoader(ClassLoader loader)
    {
        if (isStarted())
            throw new IllegalStateException("Started");
        _loader=loader;
    }


    /* ------------------------------------------------------------ */
    /** Get the classloader.
     * If no classloader has been set and the context has been loaded
     * normally, then null is returned.
     * If no classloader has been set and the context was loaded from
     * a classloader, that loader is returned.
     * If a classloader has been set and no classpath has been set then
     * the set classloader is returned.
     * If a classloader and a classpath has been set, then a new
     * URLClassloader initialized on the classpath with the set loader as a
     * partent is return.
     * @return Classloader or null.
     */
    public synchronized ClassLoader getClassLoader()
    {
        return _loader;
    }

    /* ------------------------------------------------------------ */
    /** Set Parent ClassLoader.
     * By default the parent loader is the thread context classloader
     * of the thread that calls initClassLoader.  If setClassLoader is
     * called, then the parent is ignored.
     * @param loader The class loader to use for the parent loader of
     * the context classloader.
     */
    public synchronized void setParentClassLoader(ClassLoader loader)
    {
        if (isStarted())
            throw new IllegalStateException("Started");
        _parent=loader;
    }

    /* ------------------------------------------------------------ */
    public ClassLoader getParentClassLoader()
    {
        return _parent;
    }

    /* ------------------------------------------------------------ */
    /** Initialize the context classloader.
     * Initialize the context classloader with the current parameters.
     * Any attempts to change the classpath after this call will
     * result in a IllegalStateException
     * @param forceContextLoader If true, a ContextLoader is always if
     * no loader has been set.
     */
    protected void initClassLoader(boolean forceContextLoader)
        throws MalformedURLException, IOException
    {
        if (_loader==null)
        {
            // If no parent, then try this threads classes loader as parent
            if (_parent==null)
                _parent=Thread.currentThread().getContextClassLoader();

            // If no parent, then try this classes loader as parent
            if (_parent==null)
                _parent=this.getClass().getClassLoader();

            Code.debug("Init classloader from ",_classPath,
                       ", ",_parent," for ",this);

            if (forceContextLoader || _classPath!=null || _permissions!=null)
            {
                ContextLoader loader=new ContextLoader(this,_classPath,_parent,_permissions);
                loader.setJava2Compliant(_classLoaderJava2Compliant);
                _loader=loader;
            }
            else
                _loader=_parent;
        }
    }

    /* ------------------------------------------------------------ */
    public synchronized Class loadClass(String className)
        throws ClassNotFoundException
    {
        if (_loader==null)
        {
            try{initClassLoader(false);}
            catch(Exception e)
            {
                Code.warning(e);
                return null;
            }
        }

        if (className==null)
            return null;

        return _loader.loadClass(className);
    }

    /* ------------------------------------------------------------ */
    /** set error page URI.
     * @param error A string representing an error code or a
     * exception classname
     * @param uriInContext
     */
    public void setErrorPage(String error,String uriInContext)
    {
        if (_errorPages==null)
            _errorPages=new HashMap(5);
        _errorPages.put(error,uriInContext);
    }

    /* ------------------------------------------------------------ */
    /** get error page URI.
     * @param error A string representing an error code or a
     * exception classname
     * @return URI within context
     */
    public String getErrorPage(String error)
    {
        if (_errorPages==null)
            return null;
       return (String) _errorPages.get(error);
    }


    /* ------------------------------------------------------------ */
    public String removeErrorPage(String error)
    {
        if (_errorPages==null)
            return null;
       return (String) _errorPages.remove(error);
    }

    /* ------------------------------------------------------------ */
    /** Set the realm name.
     * @param realmName The name to use to retrieve the actual realm
     * from the HttpServer
     */
    public void setRealmName(String realmName)
    {
        _realmName=realmName;
    }

    /* ------------------------------------------------------------ */
    public String getRealmName()
    {
        return _realmName;
    }

    /* ------------------------------------------------------------ */
    /** Set the  realm.
     */
    public void setRealm(UserRealm realm)
    {
        _userRealm=realm;
    }

    /* ------------------------------------------------------------ */
    public UserRealm getRealm()
    {
        return _userRealm;
    }

    /* ------------------------------------------------------------ */
    public Authenticator getAuthenticator()
    {
        return _authenticator;
    }

    /* ------------------------------------------------------------ */
    public void setAuthenticator(Authenticator authenticator)
    {
        _authenticator=authenticator;
    }

    /* ------------------------------------------------------------ */
    public void addSecurityConstraint(String pathSpec, SecurityConstraint sc)
    {
        List scs = (List)_constraintMap.get(pathSpec);
        if (scs==null)
        {
            scs=new ArrayList(2);
            _constraintMap.put(pathSpec,scs);
        }
        scs.add(sc);

        Code.debug("added ",sc," at ",pathSpec);
    }

    /* ------------------------------------------------------------ */
    public boolean isAuthConstrained()
    {
        Iterator i = _constraintMap.values().iterator();
        while(i.hasNext())
        {
            Iterator j= ((ArrayList)i.next()).iterator();
            while(j.hasNext())
            {
                SecurityConstraint sc = (SecurityConstraint)j.next();
                if (sc.isAuthenticate())
                {
                    return true;
                }
            }
        }
        return false;
    }


    /* ------------------------------------------------------------ */
    public boolean checkSecurityConstraints(String pathInContext,
                                            HttpRequest request,
                                            HttpResponse response)
        throws HttpException, IOException
    {
        UserRealm realm = getRealm();

        // Get all path matches
        List scss =_constraintMap.getMatches(pathInContext);
        if (scss!=null)
        {
            Code.debug("Security Constraint on ",pathInContext," against ",scss);

            // for each path match
            matches:
            for (int m=0;m<scss.size();m++)
            {
                // Get all constraints
                Map.Entry entry=(Map.Entry)scss.get(m);
                if (Code.verbose())
                    Code.debug("Check ",pathInContext," against ",entry);

                List scs = (List)entry.getValue();

                switch (SecurityConstraint.check(scs,
                                                 _authenticator,
                                                 realm,
                                                 pathInContext,
                                                 request,
                                                 response))
                {
                  case -1: return false; // Auth failed.
                  case 0: continue; // No constraints matched
                  case 1: break matches; // Passed a constraint.
                }
            }
        }

        return true;
    }

    /* ------------------------------------------------------------ */
    /** Get the map of mime type to char encoding.
     * @return Map of mime type to character encodings.
     */
    public synchronized Map getEncodingMap()
    {
        if (_encodingMap==null)
            _encodingMap=Collections.unmodifiableMap(__encodings);
        return _encodingMap;
    }

    /* ------------------------------------------------------------ */
    /** Set the map of mime type to char encoding.
     * Also sets the org.mortbay.http.encodingMap context attribute
     * @param encodingMap Map of mime type to character encodings.
     */
    public void setEncodingMap(Map encodingMap)
    {
        _encodingMap = encodingMap;
    }

    /* ------------------------------------------------------------ */
    /** Get char encoding by mime type.
     * @param type A mime type.
     * @return The prefered character encoding for that type if known.
     */
    public String getEncodingByMimeType(String type)
    {
        String encoding =null;

        if (type!=null)
            encoding=(String)_encodingMap.get(type);

        return encoding;
    }

    /* ------------------------------------------------------------ */
    /** Set the encoding that should be used for a mimeType.
     * @param mimeType
     * @param encoding
     */
    public void setTypeEncoding(String mimeType,String encoding)
    {
        getEncodingMap().put(mimeType,encoding);
    }

    /* ------------------------------------------------------------ */
    /** Set null path redirection.
     * @param b if true a /context request will be redirected to
     * /context/ if there is not path in the context.
     */
    public void setRedirectNullPath(boolean b)
    {
        _redirectNullPath=b;
    }

    /* ------------------------------------------------------------ */
    /**
     * @return True if a /context request is redirected to /context/ if
     * there is not path in the context.
     */
    public boolean isRedirectNullPath()
    {
        return _redirectNullPath;
    }



    /* ------------------------------------------------------------ */
    /** Set the permissions to be used for this context.
     * The collection of permissions set here are used for all classes
     * loaded by this context.  This is simpler that creating a
     * security policy file, as not all code sources may be statically
     * known.
     * @param permissions
     */
    public void setPermissions(PermissionCollection permissions)
    {
        _permissions=permissions;
    }

    /* ------------------------------------------------------------ */
    /** Get the permissions to be used for this context.
     */
    public PermissionCollection getPermissions()
    {
        return _permissions;
    }

    /* ------------------------------------------------------------ */
    /** Add a permission to this context.
     * The collection of permissions set here are used for all classes
     * loaded by this context.  This is simpler that creating a
     * security policy file, as not all code sources may be statically
     * known.
     * @param permission
     */
    public void addPermission(Permission permission)
    {
        if (_permissions==null)
            _permissions=new Permissions();
        _permissions.add(permission);
    }

    /* ------------------------------------------------------------ */
    /** Handler request.
     * Determine the path within the context and then call
     * handle(pathInContext,request,response).
     * @param request
     * @param response
     * @return True if the request has been handled.
     * @exception HttpException
     * @exception IOException
     */
    public boolean handle(HttpRequest request,
                          HttpResponse response)
        throws HttpException, IOException
    {
        if (!_started)
            return false;

        // reject requests by real host
        if (_hosts!=null && _hosts.size()>0)
        {
            Object o = request.getHttpConnection().getConnection();
            if (o instanceof Socket)
            {
                Socket s=(Socket)o;
                if (!_hosts.contains(s.getLocalAddress()))
                {
                    Code.debug(s.getLocalAddress()," not in ",_hosts);
                    return false;
                }
            }
        }
        
        // handle stats
        if (_statsOn)
        {
            synchronized(_statsLock)
            {
                _requests++;
                _requestsActive++;
                if (_requestsActive>_requestsActiveMax)
                    _requestsActiveMax=_requestsActive;
            }
        }

        String pathInContext = URI.canonicalPath(request.getPath());
        if (pathInContext==null)
        {
            // Must be a bad request.
            throw new HttpException(HttpResponse.__400_Bad_Request);
        }

        if (_contextPath.length()>1)
            pathInContext=pathInContext.substring(_contextPath.length());

        if (_redirectNullPath && (pathInContext==null ||
                                  pathInContext.length()==0))
        {
            StringBuffer buf=request.getRequestURL();
            buf.append("/");
            String q=request.getQuery();
            if (q!=null&&q.length()!=0)
                buf.append("?"+q);
            response.setField(HttpFields.__Location,
                              buf.toString());
            if (Code.debug())
                Code.warning(this+" consumed all of path "+
                             request.getPath()+
                             ", redirect to "+buf.toString());
            response.sendError(302);
            return true;
        }

        String pathParams=null;
        int semi = pathInContext.lastIndexOf(';');
        if (semi>=0)
        {
            int pl = pathInContext.length()-semi;
            String ep=request.getEncodedPath();
            if(';'==ep.charAt(ep.length()-pl))
            {
                pathParams=pathInContext.substring(semi+1);
                pathInContext=pathInContext.substring(0,semi);
            }
        }

        try
        {
            return handle(pathInContext,pathParams,request,response);
        }
        finally
        {
            UserPrincipal user = request.getUserPrincipal();
            if (_userRealm!=null)
                _userRealm.disassociate(user);
        }
    }

    /* ------------------------------------------------------------ */
    /** Handler request.
     * Call each HttpHandler until request is handled.
     * @param pathInContext Path in context
     * @param pathParams Path parameters such as encoded Session ID
     * @param request
     * @param response
     * @return True if the request has been handled.
     * @exception HttpException
     * @exception IOException
     */
    public boolean handle(String pathInContext,
                          String pathParams,
                          HttpRequest request,
                          HttpResponse response)
        throws HttpException, IOException
    {
        // Save the thread context loader
        Thread thread = Thread.currentThread();
        ClassLoader lastContextLoader=thread.getContextClassLoader();
        HttpContext lastHttpContext=response.getHttpContext();
        try
        {
            if (_loader!=null)
                thread.setContextClassLoader(_loader);
            response.setHttpContext(this);

            HttpHandler[] handlers=getHandlers();
            for (int k=0;k<handlers.length;k++)
            {
                HttpHandler handler = handlers[k];

                if (!handler.isStarted())
                {
                    Code.debug(handler," not started in ",this);
                    continue;
                }

                Code.debug("Handler ",handler);

                handler.handle(pathInContext,
                               pathParams,
                               request,
                               response);

                if (request.isHandled())
                {
                    Code.debug("Handled by ",handler);
                    return true;
                }
            }
            return false;
        }
        finally
        {
            thread.setContextClassLoader(lastContextLoader);
            response.setHttpContext(lastHttpContext);
        }
    }

    /* ------------------------------------------------------------ */
    public String getHttpContextName()
    {
        if (_contextName==null)
            _contextName = (_vhosts.size()>1?(_vhosts.toString()+":"):"")+_contextPath;
        return _contextName;
    }

    /* ------------------------------------------------------------ */
    public String toString()
    {
        return "HttpContext["+getHttpContextName()+"]";
    }

    /* ------------------------------------------------------------ */
    public String toString(boolean detail)
    {
        return "HttpContext["+getHttpContextName()+"]" +
            (detail?("="+_handlers):"");
    }

    /* ------------------------------------------------------------ */
    public synchronized void start()
        throws Exception
    {
        if (isStarted())
            return;

        statsReset();

        if (_httpServer==null)
            throw new IllegalStateException("No server for "+this);

        // start the context itself
        getMimeMap();
        getEncodingMap();

        // Setup realm
        if (_userRealm==null && _authenticator!=null)
        {
            _userRealm=_httpServer.getRealm(_realmName);
            if (_userRealm==null)
                Code.warning("No Realm: "+_realmName);
        }

        // setup the context loader
        initClassLoader(false);

        // Set attribute if needed
        String attr = getInitParameter(__fileClassPathAttr);
        if (attr!=null && attr.length()>0)
            setAttribute(attr,getFileClassPath());

        // Start the handlers
        Thread thread = Thread.currentThread();
        ClassLoader lastContextLoader=thread.getContextClassLoader();
        try
        {
            if (_loader!=null)
                thread.setContextClassLoader(_loader);

            if (_requestLog!=null)
                _requestLog.start();
            
            startHandlers();
        }
        finally
        {
            thread.setContextClassLoader(lastContextLoader);
            _started=true;
        }

        Log.event("Started "+this);
    }

    /* ------------------------------------------------------------ */
    /** Start the handlers.
     * This is called by start after the classloader has been
     * initialized and set as the thread context loader.
     * It may be specialized to provide custom handling
     * before any handlers are started.
     * @exception Exception
     */
    protected void startHandlers()
        throws Exception
    {
        // Prepare a multi exception
        MultiException mx = new MultiException();

        Iterator handlers = _handlers.iterator();
        while(handlers.hasNext())
        {
            HttpHandler handler=(HttpHandler)handlers.next();
            if (!handler.isStarted())
                try{handler.start();}catch(Exception e){mx.add(e);}
        }
        mx.ifExceptionThrow();
    }

    /* ------------------------------------------------------------ */
    public synchronized boolean isStarted()
    {
        return _started;
    }

    /* ------------------------------------------------------------ */
    /** Stop the context.
     * @param graceful If true and statistics are on, then this method will wait
     * for requestsActive to go to zero before calling stop()
     */
    public void stop(boolean graceful)
        throws InterruptedException
    {
        _started=false;

        // wait for all requests to complete.
        while (graceful && _statsOn && _requestsActive>0 && _httpServer!=null)
            try {Thread.sleep(100);}
            catch (InterruptedException e){throw e;}
            catch (Exception e){Code.ignore(e);}

        stop();
    }

    /* ------------------------------------------------------------ */
    /** Stop the context.
     */
    public void stop()
        throws InterruptedException
    {
        _started=false;

        if (_httpServer==null)
            throw new InterruptedException("Destroy called");

        synchronized(this)
        {
            // Notify the container for the stop
            Thread thread = Thread.currentThread();
            ClassLoader lastContextLoader=thread.getContextClassLoader();
            try
            {
                if (_loader!=null)
                    thread.setContextClassLoader(_loader);
                Iterator handlers = _handlers.iterator();
                while(handlers.hasNext())
                {
                    HttpHandler handler=(HttpHandler)handlers.next();
                    if (handler.isStarted())
                    {
                        try{handler.stop();}
                        catch(Exception e){Code.warning(e);}
                    }
                }
                
                if (_requestLog!=null)
                    _requestLog.stop();
            }
            finally
            {
                thread.setContextClassLoader(lastContextLoader);
            }
            _loader=null;
        }
        _cache.clear();
	_constraintMap.clear();
        if (_attributes!=null)
            _attributes.clear();
        Log.event("Stopped "+this);
    }


    /* ------------------------------------------------------------ */
    /** Destroy a context.
     * Destroy a context and remove it from the HttpServer. The
     * HttpContext must be stopped before it can be destroyed.
     */
    public void destroy()
    {
        if (isStarted())
            throw new IllegalStateException("Started");

        if (_httpServer!=null)
            _httpServer.removeContext(this);

        _httpServer=null;
        if (_handlers!=null)
            _handlers.clear();
        _handlers=null;
        _parent=null;
        _loader=null;
        _resourceBase=null;
        _attributes=null;
        if (_initParams!=null)
            _initParams.clear();
        _initParams=null;
        if (_vhosts!=null)
            _vhosts.clear();
        _vhosts=null;
        _hosts=null;
        _tmpDir=null;

        setMimeMap(null);
        _encodingMap=null;
        if (_errorPages!=null)
            _errorPages.clear();
        _errorPages=null;

        _permissions=null;
    }


    /* ------------------------------------------------------------ */
    /** Set the request log.
     * @param log RequestLog to use.
     */
    public void setRequestLog(RequestLog log)
    {
        _requestLog=log;
    }
    
    /* ------------------------------------------------------------ */
    public RequestLog getRequestLog()
    {
        return _requestLog;
    }

    /* ------------------------------------------------------------ */
    /** True set statistics recording on for this context.
     * @param on If true, statistics will be recorded for this context.
     */
    public void setStatsOn(boolean on)
    {
        Log.event("setStatsOn "+on+" for "+this);
        _statsOn=on;
        statsReset();
    }

    /* ------------------------------------------------------------ */
    public boolean getStatsOn() {return _statsOn;}

    /* ------------------------------------------------------------ */
    public long getStatsOnMs()
    {return _statsOn?(System.currentTimeMillis()-_statsStartedAt):0;}

    /* ------------------------------------------------------------ */
    public void statsReset()
    {
        synchronized(_statsLock)
        {
            if (_statsOn)
                _statsStartedAt=System.currentTimeMillis();
            _requests=0;
            _requestsActive=0;
            _requestsActiveMax=0;
            _responses1xx=0;
            _responses2xx=0;
            _responses3xx=0;
            _responses4xx=0;
            _responses5xx=0;
        }
    }

    /* ------------------------------------------------------------ */
    /**
     * @return Get the number of requests handled by this context
     * since last call of statsReset(). If setStatsOn(false) then this
     * is undefined.
     */
    public int getRequests() {return _requests;}

    /* ------------------------------------------------------------ */
    /**
     * @return Number of requests currently active.
     * Undefined if setStatsOn(false).
     */
    public int getRequestsActive() {return _requestsActive;}

    /* ------------------------------------------------------------ */
    /**
     * @return Maximum number of active requests
     * since statsReset() called. Undefined if setStatsOn(false).
     */
    public int getRequestsActiveMax() {return _requestsActiveMax;}

    /* ------------------------------------------------------------ */
    /**
     * @return Get the number of responses with a 2xx status returned
     * by this context since last call of statsReset(). Undefined if
     * if setStatsOn(false).
     */
    public int getResponses1xx() {return _responses1xx;}

    /* ------------------------------------------------------------ */
    /**
     * @return Get the number of responses with a 100 status returned
     * by this context since last call of statsReset(). Undefined if
     * if setStatsOn(false).
     */
    public int getResponses2xx() {return _responses2xx;}

    /* ------------------------------------------------------------ */
    /**
     * @return Get the number of responses with a 3xx status returned
     * by this context since last call of statsReset(). Undefined if
     * if setStatsOn(false).
     */
    public int getResponses3xx() {return _responses3xx;}

    /* ------------------------------------------------------------ */
    /**
     * @return Get the number of responses with a 4xx status returned
     * by this context since last call of statsReset(). Undefined if
     * if setStatsOn(false).
     */
    public int getResponses4xx() {return _responses4xx;}

    /* ------------------------------------------------------------ */
    /**
     * @return Get the number of responses with a 5xx status returned
     * by this context since last call of statsReset(). Undefined if
     * if setStatsOn(false).
     */
    public int getResponses5xx() {return _responses5xx;}


    /* ------------------------------------------------------------ */
    /** Log a request and response.
     * Statistics are also collected by this method.
     * @param request
     * @param response
     */
    public void log(HttpRequest request,
                    HttpResponse response,
                    int length)
    {
        if (_statsOn)
        {
            synchronized(_statsLock)
            {
                if (--_requestsActive<0)
                    _requestsActive=0;

                if (response!=null)
                {
                    switch(response.getStatus()/100)
                    {
                      case 1: _responses1xx++;break;
                      case 2: _responses2xx++;break;
                      case 3: _responses3xx++;break;
                      case 4: _responses4xx++;break;
                      case 5: _responses5xx++;break;
                    }
                }
            }
        }

        if (_requestLog!=null &&
            request!=null &&
            response!=null)
            _requestLog.log(request,response,length);
        else if (_httpServer!=null)
            _httpServer.log(request,response,length);
    }

    /* ------------------------------------------------------------ */
    /** Get Resource MetaData.
     * This is a temp method until the resource cache is split out from the HttpContext.
     * @param resource 
     * @return Meta data for the resource.
     */
    public ResourceMetaData getResourceMetaData(Resource resource)
    {
        Object o=resource.getAssociate();
        if (o instanceof ResourceMetaData)
            return (ResourceMetaData)o;
        return new ResourceMetaData(resource);
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /** MetaData associated with a context Resource.
     */
    public class ResourceMetaData
    {
        protected String _name;
        protected Resource _resource;

        ResourceMetaData(Resource resource)
        {
            _resource=resource;
            _name=_resource.toString();
            _resource.setAssociate(this);
        }

        public String getLength()
        {
            return Long.toString(_resource.length());
        }

        public String getLastModified()
        {
            return HttpFields.__dateSend.format(new Date(_resource.lastModified()));
        }

        public String getEncoding()
        {
            return getMimeByExtension(_name);
        }
    }

    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class CachedMetaData extends ResourceMetaData
    {
        String _lastModified;
        String _encoding;
        String _length;
        String _key;

        CachedResource _cached;
        CachedMetaData _prev;
        CachedMetaData _next;

        CachedMetaData(CachedResource resource, String pathInContext)
        {
            super(resource);
            _cached=resource;
            _length=super.getLength();
            _lastModified=super.getLastModified();
            _encoding=super.getEncoding();
            _key=pathInContext;

            _next=_mostRecentlyUsed;
            _mostRecentlyUsed=this;
            if (_next!=null)
                _next._prev=this;
            _prev=null;
            if (_leastRecentlyUsed==null)
                _leastRecentlyUsed=this;

            _cache.put(_key,resource);

            _cacheSize+=_cached.length();

        }

        public String getLength()
        {
            return _length;
        }

        public String getLastModified()
        {
            return _lastModified;
        }

        public String getEncoding()
        {
            return _encoding;
        }

        /* ------------------------------------------------------------ */
        boolean isValid()
            throws IOException
        {
            if (_cached.isUptoDate())
            {
                if (_mostRecentlyUsed!=this)
                {
                    CachedMetaData tp = _prev;
                    CachedMetaData tn = _next;

                    _next=_mostRecentlyUsed;
                    _mostRecentlyUsed=this;
                    if (_next!=null)
                        _next._prev=this;
                    _prev=null;

                    if (tp!=null)
                        tp._next=tn;
                    if (tn!=null)
                        tn._prev=tp;

                    if (_leastRecentlyUsed==this && tp!=null)
                        _leastRecentlyUsed=tp;
                }
                return true;
            }

            invalidate();
            return false;
        }

        public void invalidate()
        {
            // Invalidate it
            _cache.remove(_key);
            _cacheSize=_cacheSize-(int)_cached.length();


            if (_mostRecentlyUsed==this)
                _mostRecentlyUsed=_next;
            else
                _prev._next=_next;

            if (_leastRecentlyUsed==this)
                _leastRecentlyUsed=_prev;
            else
                _next._prev=_prev;

            _prev=null;
            _next=null;
        }
    }
}
