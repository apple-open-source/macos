/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.netboot;

import java.io.IOException;
import java.io.PrintWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Iterator;
import java.util.Set;
import java.util.TimeZone;
import javax.servlet.Filter;
import javax.servlet.FilterChain;
import javax.servlet.FilterConfig;
import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.ServletRequest;
import javax.servlet.ServletResponse;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

/**
 * <em>Very</em> simple filter to support WebDAV access to resources.
 * Only intended to support the DavURLLister.
 */
public class TrivialDavFilter implements Filter {
   private ServletContext context;
   private SimpleDateFormat sharedFormat;

   public void init(FilterConfig filterConfig) throws ServletException {
      context = filterConfig.getServletContext();
      sharedFormat = new SimpleDateFormat("EEE, d MMM yyyy HH:mm:ss z", java.util.Locale.US);
      sharedFormat.setTimeZone(TimeZone.getTimeZone("GMT"));
   }

   public void doFilter(ServletRequest request, ServletResponse response, FilterChain chain) throws IOException, ServletException {
      if (request instanceof HttpServletRequest == false) {
         chain.doFilter(request, response);
         return;
      }


      HttpServletRequest hrequest = (HttpServletRequest) request;
      String method = hrequest.getMethod();
      if ("PROPFIND".equals(method)) {
         doPropfind(hrequest, (HttpServletResponse) response);
      } else {
         chain.doFilter(request, response);
      }
   }

   public void destroy() {
      sharedFormat = null;
      context = null;
   }

   private void doPropfind(HttpServletRequest request, HttpServletResponse response) throws IOException {
      String contextPath = request.getContextPath();

      // get the path from the request URI - Jetty does not return a valid
      // servlet path if this filter is mapped to '/'
      String path = request.getRequestURI().substring(contextPath.length());

      // keep WEB-INF hidden
      // also check for missing resources (Jetty does not report this below)
      if (path.startsWith("/WEB-INF") || context.getResource(path) == null) {
         response.sendError(HttpServletResponse.SC_NOT_FOUND);
         return;
      }

      Set resourcePaths = context.getResourcePaths(path);
      if (resourcePaths == null) {
         // not found -- should not happen due to check above
         // not all containers seem to return null here
         response.sendError(HttpServletResponse.SC_NOT_FOUND);
         return;
      }

      // SimpleDateFormat is not thread-safe, so clone here.
      SimpleDateFormat dateFormat = (SimpleDateFormat) sharedFormat.clone();

      response.setStatus(207); // DAV MultiStatus Response
      PrintWriter out = response.getWriter();
      out.println("<?xml version='1.0' encoding='utf-8' ?>");
      out.println("<D:multistatus xmlns:D='DAV:'>");

      if (resourcePaths.isEmpty()) {
         // not a collection - do the resource itself
         writeResponse(out, contextPath, path, dateFormat);
      } else {
         for (Iterator i = resourcePaths.iterator(); i.hasNext();) {
            String s = (String) i.next();
            writeResponse(out, contextPath, s, dateFormat);
         }
      }
      out.println("</D:multistatus>");
      out.flush();
      response.flushBuffer();
   }

   private void writeResponse(PrintWriter out, String contextPath, String path, SimpleDateFormat dateFormat) throws IOException {
      Date lastModified = new Date(context.getResource(path).openConnection().getLastModified());
      out.println("<D:response>");
      out.println("<D:href>" + contextPath + path + "</D:href>");
      out.println("<D:propstat>");
      out.println("<D:prop>");
      if (path.endsWith("/")) {
         out.println("<D:resourcetype><D:collection/></D:resourcetype>");
      } else {
         out.println("<D:resourcetype/>");
      }
      out.println("<D:getlastmodified>" + dateFormat.format(lastModified) + "</D:getlastmodified>");
      out.println("</D:prop>");
      out.println("<D:status>HTTP/1.1 200 OK</D:status>");
      out.println("</D:propstat>");
      out.println("</D:response>");
   }
}
