// ===========================================================================
// Copyright (c) 1996 Mort Bay Consulting Pty. Ltd. All rights reserved.
// $Id: WebApplicationHandler.java,v 1.5.2.13 2003/06/04 04:47:52 starksm Exp $
// ---------------------------------------------------------------------------

package org.mortbay.jetty.servlet;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.UnavailableException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import org.mortbay.http.HttpResponse;
import org.mortbay.http.HttpContext;
import org.mortbay.util.Code;
import org.mortbay.util.LazyList;
import org.mortbay.util.MultiException;
import org.mortbay.util.MultiMap;
import org.mortbay.util.StringUtil;

/* --------------------------------------------------------------------- */
/** WebApp HttpHandler.
 * This handler extends the ServletHandler with security, filter and resource
 * capabilities to provide full J2EE web container support.
 * <p>
 * @since Jetty 4.1
 * @see org.mortbay.jetty.servlet.WebApplicationContext
 * @version $Id: WebApplicationHandler.java,v 1.5.2.13 2003/06/04 04:47:52 starksm Exp $
 * @author Greg Wilkins
 */
public class WebApplicationHandler extends ServletHandler 
{
    private Map _filterMap=new HashMap();
    private List _pathFilters=new ArrayList();
    private List _filters=new ArrayList();
    private MultiMap _servletFilterMap=new MultiMap();
    private boolean _acceptRanges=true;
    
    private transient boolean _started=false;
    
    /* ------------------------------------------------------------ */
    public boolean isAcceptRanges()
    {
        return _acceptRanges;
    }
    
    /* ------------------------------------------------------------ */
    /** Set if the handler accepts range requests.
     * Default is false;
     * @param ar True if the handler should accept ranges
     */
    public void setAcceptRanges(boolean ar)
    {
        _acceptRanges=ar;
    }
    
    /* ------------------------------------------------------------ */
    public FilterHolder defineFilter(String name, String className)
    {
        FilterHolder holder = new FilterHolder(this,name,className);
        _filterMap.put(holder.getName(),holder);
        _filters.add(holder);
        return holder;
    }
    
    /* ------------------------------------------------------------ */
    public FilterHolder getFilter(String name)
    {
        return (FilterHolder)_filterMap.get(name);
    }

    /* ------------------------------------------------------------ */
    public FilterHolder mapServletToFilter(String servletName,
                                           String filterName)
    {
        FilterHolder holder =(FilterHolder)_filterMap.get(filterName);
        if (holder==null)
            throw new IllegalArgumentException("Unknown filter :"+filterName);
        Code.debug("Filter servlet ",servletName," --> ",filterName);
        _servletFilterMap.add(servletName,holder);
        holder.addServlet(servletName);
        return holder;
    }
    
    /* ------------------------------------------------------------ */
    public List getFilters()
    {
        return _filters;
    }
    
    /* ------------------------------------------------------------ */
    public FilterHolder mapPathToFilter(String pathSpec,
                                        String filterName)
    {
        FilterHolder holder =(FilterHolder)_filterMap.get(filterName);
        if (holder==null)
            throw new IllegalArgumentException("Unknown filter :"+filterName);
        
        Code.debug("Filter path ",pathSpec," --> ",filterName);

        if (!holder.isMappedToPath())
            _pathFilters.add(holder);
        holder.addPathSpec(pathSpec);
        
        return holder;
    }

    
    /* ------------------------------------------------------------ */
    public boolean isStarted()
    {
        return _started&&super.isStarted();
    }
    
    /* ----------------------------------------------------------------- */
    public synchronized void start()
        throws Exception
    {
        // Start Servlet Handler
        super.start();
        Code.debug("Path Filters: ",_pathFilters);
        Code.debug("Servlet Filters: ",_servletFilterMap);
        _started=true;
    }
    
    /* ------------------------------------------------------------ */
    public void initializeServlets()
        throws Exception
    {
        // initialize Filters
        MultiException mex = new MultiException();
        Iterator iter = _filters.iterator();
        while (iter.hasNext())
        {
            FilterHolder holder = (FilterHolder)iter.next();
            try{holder.start();}
            catch(Exception e) {mex.add(e);}
        }

        // initialize Servlets
        try {super.initializeServlets();}
        catch (Exception e){mex.add(e);}
        
        mex.ifExceptionThrow();
    }
    
    /* ------------------------------------------------------------ */
    public synchronized void stop()
        throws  InterruptedException
    {
        try
        {
            // Stop servlets
            super.stop();
            
            // Stop filters
            for (int i=_filters.size();i-->0;)
            {
                FilterHolder holder = (FilterHolder)_filters.get(i);
                holder.stop();
            }
        }
        finally
        {
            _started=false;
        }
    }

    /* ------------------------------------------------------------ */
    protected void dispatch(String pathInContext,
                            HttpServletRequest request,
                            HttpServletResponse response,
                            ServletHolder servletHolder)
        throws ServletException,
               UnavailableException,
               IOException
    {
        // Determine request type.
        int requestType=0;

        if (request instanceof Dispatcher.DispatcherRequest)
        {
            // Handle dispatch to j_security_check
            HttpContext context= getHttpContext();
            if (context!=null && context instanceof ServletHttpContext &&
                pathInContext!=null && pathInContext.endsWith(FormAuthenticator.__J_SECURITY_CHECK))
            {
                ServletHttpRequest servletHttpRequest=(ServletHttpRequest)request;
                ServletHttpResponse servletHttpResponse=(ServletHttpResponse)response;
                ServletHttpContext servletContext = (ServletHttpContext)context;
                
                if (!servletContext.jSecurityCheck(pathInContext,
                                                   servletHttpRequest.getHttpRequest(),
                                                   servletHttpResponse.getHttpResponse()))
                    return;
            }
        
            // Forward or include
            requestType=((Dispatcher.DispatcherRequest)request).getFilterType();
        }
        else
        {
            // Error or request
            ServletHttpRequest servletHttpRequest=(ServletHttpRequest)request;
            ServletHttpResponse servletHttpResponse=(ServletHttpResponse)response;
            HttpResponse httpResponse=servletHttpResponse.getHttpResponse();

            if (httpResponse.getStatus()!=HttpResponse.__200_OK)
            {
                // Error
                requestType=FilterHolder.__ERROR;
            }
            else
            {
                // Request
                requestType=FilterHolder.__REQUEST;
                // protect web-inf and meta-inf
                if (StringUtil.startsWithIgnoreCase(pathInContext,"/web-inf")  ||
                    StringUtil.startsWithIgnoreCase(pathInContext,"/meta-inf"))
                {
                    response.sendError(HttpResponse.__404_Not_Found);
                    return;
                }
                
                // Security Check
                if (!getHttpContext().checkSecurityConstraints
                    (pathInContext,
                     servletHttpRequest.getHttpRequest(),
                     httpResponse))
                    return;
            }
        }
        
        // Build list of filters
        Object filters = null;
        
        // Path filters
        if (pathInContext!=null && _pathFilters.size()>0)
        {
            for (int i=0;i<_pathFilters.size();i++)
            {
                FilterHolder holder=(FilterHolder)_pathFilters.get(i);
                if (holder.appliesTo(pathInContext,requestType))
                    filters=LazyList.add(filters,holder);
            }
        }
        
        // Servlet filters
        if (servletHolder!=null && _servletFilterMap.size()>0)
        {
            Object o=_servletFilterMap.get(servletHolder.getName());
            if (o!=null)
            {
                if (o instanceof List)
                {
                    List list=(List)o;
                    for (int i=0;i<list.size();i++)
                    {
                        FilterHolder holder = (FilterHolder)list.get(i);
                        if (holder.appliesTo(requestType))
                            filters=LazyList.add(filters,holder);
                    }
                }
                else
                {
                    FilterHolder holder = (FilterHolder)o;
                    if (holder.appliesTo(requestType))
                        filters=LazyList.add(filters,holder);
                } 
            }
        }
        
        // Do the handling thang
        if (LazyList.size(filters)>0)
        {
            Chain chain=new Chain(pathInContext,filters,servletHolder);
            chain.doFilter(request,response);
        }
        else
        {
            // Call servlet
            if (servletHolder!=null)
            {
                if (Code.verbose()) Code.debug("call servlet ",servletHolder);
                servletHolder.handle(request,response);
            }
            else // Not found
                notFound(request,response);
        }
    }
    
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    /* ------------------------------------------------------------ */
    private class Chain implements FilterChain
    {
        String _pathInContext;
        int _filter=0;
        Object _filters;
        ServletHolder _servletHolder;

        /* ------------------------------------------------------------ */
        Chain(String pathInContext,
              Object filters,
              ServletHolder servletHolder)
        {
            _pathInContext=pathInContext;
            _filters=filters;
            _servletHolder=servletHolder;
        }
        
        /* ------------------------------------------------------------ */
        public void doFilter(ServletRequest request, ServletResponse response)
            throws IOException,
                   ServletException
        {
            if (Code.verbose()) Code.debug("doFilter ",_filter);
            
            // pass to next filter
            if (_filter<LazyList.size(_filters))
            {
                FilterHolder holder = (FilterHolder)LazyList.get(_filters,_filter++);
                if (Code.verbose()) Code.debug("call filter ",holder);
                Filter filter = holder.getFilter();
                filter.doFilter(request,response,this);
                return;
            }

            // Call servlet
            if (_servletHolder!=null)
            {
                if (Code.verbose()) Code.debug("call servlet ",_servletHolder);
                _servletHolder.handle(request,response);
            }
            else // Not found
                notFound((HttpServletRequest)request,
                         (HttpServletResponse)response);
        }
    }
}

