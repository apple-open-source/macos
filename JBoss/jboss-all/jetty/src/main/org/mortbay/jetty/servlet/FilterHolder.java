// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: FilterHolder.java,v 1.15.2.10 2003/06/04 04:47:51 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.jetty.servlet;

import java.util.Enumeration;
import java.util.Map;
import java.util.Iterator;
import javax.servlet.Filter;
import javax.servlet.FilterConfig;
import javax.servlet.ServletContext;
import org.mortbay.http.HttpHandler;
import org.mortbay.http.PathMap;
import org.mortbay.util.LazyList;

/* --------------------------------------------------------------------- */
/** 
 * @version $Id: FilterHolder.java,v 1.15.2.10 2003/06/04 04:47:51 starksm Exp $
 * @author Greg Wilkins
 */
public class FilterHolder
    extends Holder
{
    /* ------------------------------------------------------------ */
    public static final int
        __REQUEST=1,
        __FORWARD=2,
        __INCLUDE=4,
        __ERROR=8;

    public static int type(String type)
    {
        if ("request".equalsIgnoreCase(type))
            return __REQUEST;
        if ("forward".equalsIgnoreCase(type))
            return __FORWARD;
        if ("include".equalsIgnoreCase(type))
            return __INCLUDE;
        if ("error".equalsIgnoreCase(type))
            return __ERROR;
        return 0;
    }
    
    /* ------------------------------------------------------------ */
    private PathMap _pathSpecs;
    private int _appliesTo;
    private Object _servlets;

    private transient Filter _filter;
    private transient Config _config;
        
    /* ---------------------------------------------------------------- */
    /** Constructor for Serialization.
     */
    public FilterHolder()
    {}
    
    /* ---------------------------------------------------------------- */
    public FilterHolder(HttpHandler httpHandler,
                        String name,
                        String className)
    {
        super(httpHandler,name,className);
    }

    /* ------------------------------------------------------------ */
    /** Add a type that this filter applies to.
     * @param type Of __REQUEST, __FORWARD, __INCLUDE or __ERROR
     */
    public void addAppliesTo(int type)
    {
        _appliesTo|=type;
    }

    /* ------------------------------------------------------------ */
    /** Add a type that this filter applies to.
     * @param type "REQUEST", "FORWARD", "INCLUDE" or "ERROR"
     */
    public void addAppliesTo(String type)
    {
        _appliesTo|=type(type);
    }
    
    /* ------------------------------------------------------------ */
    /** Add A servlet that this filter applies to.
     * @param servlet 
     */
    public void addServlet(String servlet)
    {
        _servlets=LazyList.add(_servlets,servlet);
    }
    
    /* ------------------------------------------------------------ */
    /** Add A path spec that this filter applies to.
     * @param pathSpec 
     */
    public void addPathSpec(String pathSpec)
    {
        if (_pathSpecs==null)
            _pathSpecs=new PathMap();
        _pathSpecs.put(pathSpec,pathSpec);
    }
    
    /* ------------------------------------------------------------ */
    public boolean isMappedToPath()
    {
        return _pathSpecs!=null;
    }

    /* ------------------------------------------------------------ */
    /** Check if this filter applies.
     * @param type The type of request: __REQUEST,__FORWARD,__INCLUDE or __ERROR.
     * @return True if this filter applies
     */
    public boolean appliesTo(int type)
    {
        return  (_appliesTo&type)!=0 || _appliesTo==0&&type==__REQUEST ;
    }
    
    /* ------------------------------------------------------------ */
    /** Check if this filter applies to a path.
     * @param path The path to check.
     * @param type The type of request: __REQUEST,__FORWARD,__INCLUDE or __ERROR.
     * @return True if this filter applies
     */
    public boolean appliesTo(String path, int type)
    {
        return
            ((_appliesTo&type)!=0 || _appliesTo==0&&type==__REQUEST ) &&
            _pathSpecs!=null &&
            _pathSpecs.getMatch(path)!=null;
    }
    
    /* ------------------------------------------------------------ */
    public String appliedPathSpec(String path)
    {
        if (_pathSpecs==null)
            return null;
        Map.Entry entry = _pathSpecs.getMatch(path);
        if (entry==null)
            return null;
        return (String)entry.getKey();
    }

    /* ------------------------------------------------------------ */
    public void start()
        throws Exception
    {
        super.start();
        
        if (!javax.servlet.Filter.class
            .isAssignableFrom(_class))
        {
            super.stop();
            throw new IllegalStateException(_class+" is not a javax.servlet.Filter");
        }

        _filter=(Filter)newInstance();
        _config=new Config();
        _filter.init(_config);
    }

    /* ------------------------------------------------------------ */
    public void stop()
    {
        if (_filter!=null)
            _filter.destroy();
        _filter=null;
        _config=null;
        super.stop();   
    }
    
    /* ------------------------------------------------------------ */
    public Filter getFilter()
    {
        return _filter;
    }

    /* ------------------------------------------------------------ */
    public String[] getPaths()
    {
        if (_pathSpecs==null)
            return null;
        int s = _pathSpecs.keySet().size();
        return (String[]) _pathSpecs.keySet().toArray(new String[s]);
    }
    
    /* ------------------------------------------------------------ */
    public String[] getServlets()
    {
        if (_servlets==null)
            return null;
        int s = LazyList.size(_servlets);
        return (String[])LazyList.getList(_servlets).toArray(new String[s]);
    }
    
    /* ------------------------------------------------------------ */
    public String toString()
    {
        StringBuffer buf = new StringBuffer();
        buf.append(getName());
        buf.append('[');
        buf.append(getClassName());
        for (int i=0;i<LazyList.size(_servlets);i++)
        {
            buf.append(',');
            buf.append(LazyList.get(_servlets,i));
        }
        if (_pathSpecs!=null)
        {
            Iterator iter = _pathSpecs.keySet().iterator();
            while (iter.hasNext())
            {
                buf.append(',');
                buf.append(iter.next());
            }
        }
        buf.append(']');
        return buf.toString();
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    class Config implements FilterConfig
    {
        /* ------------------------------------------------------------ */
        public String getFilterName()
        {
            return FilterHolder.this.getName();
        }

        /* ------------------------------------------------------------ */
        public ServletContext getServletContext()
        {
            return ((WebApplicationHandler)_httpHandler).getServletContext();
        }
        
        /* -------------------------------------------------------- */
        public String getInitParameter(String param)
        {
            return FilterHolder.this.getInitParameter(param);
        }
    
        /* -------------------------------------------------------- */
        public Enumeration getInitParameterNames()
        {
            return FilterHolder.this.getInitParameterNames();
        }
    }
    
}





