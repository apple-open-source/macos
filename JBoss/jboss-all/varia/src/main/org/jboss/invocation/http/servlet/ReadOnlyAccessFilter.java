/*
 * NamingAccessFilter.java
 *
 * Created on September 17, 2002, 8:22 AM
 */
package org.jboss.invocation.http.servlet;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.lang.reflect.Method;
import java.security.Principal;
import java.util.Map;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletInputStream;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;

import org.jboss.invocation.MarshalledInvocation;
import org.jboss.logging.Logger;
import org.jboss.mx.util.MBeanServerLocator;

/** A serlvet filter that enforces read-only access to a single context
 * given by the readOnlyContext init-parameter.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.3 $
 */
public class ReadOnlyAccessFilter implements Filter
{
   private static Logger log = Logger.getLogger(ReadOnlyAccessFilter.class);
   private FilterConfig filterConfig = null;
   private String readOnlyContext;
   private Map namingMethodMap;

   /** Init method for this filter
    */
   public void init(FilterConfig filterConfig)
      throws ServletException
   {
      this.filterConfig = filterConfig;
      if (filterConfig != null)
      {
         readOnlyContext = filterConfig.getInitParameter("readOnlyContext");
         String invokerName = filterConfig.getInitParameter("invokerName");
         try
         {
            // Get the Naming interface method map from the invoker
            MBeanServer mbeanServer = MBeanServerLocator.locateJBoss();
            ObjectName mbean = new ObjectName(invokerName);
            namingMethodMap = (Map) mbeanServer.getAttribute(mbean, "MethodMap");
         }
         catch(Exception e)
         {
            log.error("Failed to init ReadOnlyAccessFilter", e);
            throw new ServletException("Failed to init ReadOnlyAccessFilter", e);
         }
      }
   }

   /** Intercept requests and validate that requests to the NamingService
    *
    * @param request The servlet request we are processing
    * @param result The servlet response we are creating
    * @param chain The filter chain we are processing
    *
    * @exception IOException if an input/output error occurs
    * @exception ServletException if a servlet error occurs
    */
   public void doFilter(ServletRequest request, ServletResponse response,
      FilterChain chain)
      throws IOException, ServletException
   {
      HttpServletRequest httpRequest = (HttpServletRequest) request;
      Principal user = httpRequest.getUserPrincipal();
      // If there was a read-only context specified validate access
      if( user == null && readOnlyContext != null )
      {
         // Extract the invocation
         ServletInputStream sis = request.getInputStream();
         ObjectInputStream ois = new ObjectInputStream(sis);
         MarshalledInvocation mi = null;
         try
         {
            mi = (MarshalledInvocation) ois.readObject();
         }
         catch(ClassNotFoundException e)
         {
            throw new ServletException("Failed to read MarshalledInvocation", e);
         }
         request.setAttribute("MarshalledInvocation", mi);
         /* Get the invocation method. If there is no method then this must
            be an invocation on an mbean other than our invoker so let it go
         */
         mi.setMethodMap(namingMethodMap);
         Method m = mi.getMethod();
         if( m != null )
            validateAccess(m, mi);
      }

      chain.doFilter(request, response);
   }

   public void destroy()
   {
   }

   /** Return a String representation of the filter
    */
   public String toString()
   {      
      if (filterConfig == null)
         return ("NamingAccessFilter()");
      StringBuffer sb = new StringBuffer("NamingAccessFilter(");
      sb.append(filterConfig);
      sb.append(")");
      return sb.toString();
   }

   private void validateAccess(Method m, MarshalledInvocation mi)
      throws ServletException
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("Checking against readOnlyContext: "+readOnlyContext);
      String methodName = m.getName();
      if( methodName.equals("lookup") == false )
         throw new ServletException("Only lookups against "+readOnlyContext+" are allowed");
      // Validate this is a lookup under readOnlyContext
      Object[] args = mi.getArguments();
      Object arg = args.length > 0 ? args[0] : "";
      String name;
      if( arg instanceof String )
         name = (String) arg;
      else
         name = arg.toString();
      if( trace )
         log.trace("Checking lookup("+name+") against: "+readOnlyContext);
      if( name.startsWith(readOnlyContext) == false )
         throw new ServletException("Lookup("+name+") is not under: "+readOnlyContext);
   }
}
