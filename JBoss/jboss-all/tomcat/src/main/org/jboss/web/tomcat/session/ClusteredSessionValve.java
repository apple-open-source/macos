/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.session;

import java.io.IOException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpSession;
import javax.servlet.ServletException;

import org.apache.catalina.valves.ValveBase;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.*;

/**
 This Valve detects all sessions that were used in a request. All sessions are given to a snapshot
 manager that handles the distribution of modified sessions.

 TOMCAT 4.1.12 UPDATE: Added findLifecycleListeners() to comply with the latest
 Lifecycle interface.

 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.1.1 $
 */
public class ClusteredSessionValve extends ValveBase implements Lifecycle
{
   // The info string for this Valve
   private static final String info = "ClusteredSessionValve/1.0";

   // The SnapshotManager that is associated with this Valve
   protected SnapshotManager snapshot;

   // Valve-lifecycle helper object
   protected LifecycleSupport support = new LifecycleSupport(this);

   // store the request and response object for parts of the clustering code that
   // have no direct access to this objects
   protected static ThreadLocal requestThreadLocal = new ThreadLocal();
   protected static ThreadLocal responseThreadLocal = new ThreadLocal();

   /**
    Create a new Valve.
    @param snapshot The SnapshotManager associated with this Valve
    */
   public ClusteredSessionValve(SnapshotManager snapshot)
   {
      super();
      this.snapshot = snapshot;
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
    This method gets called when the request goes through the Valve-chain. Our session replication mechanism replicates the
    session after request got through the servlet code.
    @param request The request object associated with this request.
    @param response The response object associated with this request.
    @param context The context of the Valve-invocation.
    */
   public void invoke(Request request, Response response, ValveContext context) throws IOException, ServletException
   {
      // Store the request and response object for the clustering code that has no direct access to
      // this objects
      requestThreadLocal.set(request);
      responseThreadLocal.set(response);

      // let the servlet invokation go through
      context.invokeNext(request, response);

      // --> We are now after the servlet invokation

      // Get the session
      HttpServletRequest servletRequest = (HttpServletRequest) request.getRequest();
      HttpSession session = (HttpSession) servletRequest.getSession(false);

      if (session != null && session.getId() != null)
      {
         // tell the snapshot manager that this session was modified
         snapshot.snapshot(session.getId());
      }

      // remove references to the request and response object so that the garbace collector can
      // kill them
      requestThreadLocal.set(null);
      responseThreadLocal.set(null);
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
      snapshot.start();
      support.fireLifecycleEvent(START_EVENT, this);
   }

   public void stop() throws LifecycleException
   {
      support.fireLifecycleEvent(STOP_EVENT, this);
      snapshot.stop();
   }

}
