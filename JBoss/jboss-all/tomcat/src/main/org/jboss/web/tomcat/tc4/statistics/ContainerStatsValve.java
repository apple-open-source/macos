/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc4.statistics;

import java.io.IOException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;
import javax.servlet.ServletException;

import org.apache.catalina.valves.ValveBase;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.*;
import org.jboss.web.tomcat.tc4.EmbeddedTomcatService;
import org.jboss.web.tomcat.statistics.InvocationStatistics;

/**
 This valve provides information to the container about active threads and
 resquest timings.

 @author Thomas Peuss <jboss@peuss.de>
 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.1.1 $
 */
public class ContainerStatsValve extends ValveBase implements Lifecycle
{
   /** The info string for this Valve */
   private static final String info = "ContainerStatsValve/1.0";

   /** Valve-lifecycle helper object */
   protected LifecycleSupport support = new LifecycleSupport(this);

   /** The web container stats reference */
   protected InvocationStatistics stats;

   /**
    Create a new Valve.
    @param catalina The container associated with the valve. 
    */
   public ContainerStatsValve(InvocationStatistics stats)
   {
      super();
      this.stats = stats;
   }

   /**
    Get information about this Valve.
    */
   public String getInfo()
   {
      return info;
   }

   /**
    Valve-chain handler method.
    This method gets called when the request goes through the Valve-chain. The
    activeThreadCount attribute gets incremented/decremented before/after the
    servlet invocation.

    @param request The request object associated with this request.
    @param response The response object associated with this request.
    @param context The context of the Valve-invocation.
    */
   public void invoke(Request request, Response response, ValveContext context)
      throws IOException, ServletException
   {
      long start = System.currentTimeMillis();

      // increment active thread count
      stats.callIn();

      // let the servlet invokation go through
      context.invokeNext(request, response);
     
      // decrement active thread count
      stats.callOut();

      // Update the web context invocation stats
      long end = System.currentTimeMillis();
      long elapsed = end - start;
      String webCtx = request.getContext().getName();
      stats.updateStats(webCtx, elapsed);
   }

   // Lifecylce-interface
   public void addLifecycleListener(LifecycleListener listener)
   {
      support.addLifecycleListener(listener);
   }

   public void removeLifecycleListener(LifecycleListener listener)
   {
      support.removeLifecycleListener(listener);
   }

   public LifecycleListener[] findLifecycleListeners()
   {
      return support.findLifecycleListeners();
   }

   public void start() throws LifecycleException
   {
      support.fireLifecycleEvent(START_EVENT, this);
   }

   public void stop() throws LifecycleException
   {
      support.fireLifecycleEvent(STOP_EVENT, this);
      stats = null;
   }

}
