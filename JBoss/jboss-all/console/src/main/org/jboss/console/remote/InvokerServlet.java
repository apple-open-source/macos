/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.console.remote;

import javax.servlet.ServletException;


/** This servlet accepts a post containing a MarshalledInvocation, extracts
 * the Invocation object, and then routes the invocation via JMX to either:
 * 1. the MBean specified via the invokerName ini parameter
 * 2. the MBean whose object name hash is specified by the invocation.getObjectName()
 * value. This name's hash must have been entered into the Registry.
 *
 * The method signature of the invoker must be Object invoke(org.jboss.invocation.Invocation).
 *
 * @see org.jboss.system.Registry
 * @see org.jboss.invocation.Invocation
 *
 * @author  Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.3 $
 */
public class InvokerServlet extends javax.servlet.http.HttpServlet
{
   private static org.jboss.logging.Logger log = org.jboss.logging.Logger.getLogger (InvokerServlet.class);
   /** A serialized MarshalledInvocation */
   private static String REQUEST_CONTENT_TYPE =
   "application/x-java-serialized-object; class=org.jboss.console.remote.RemoteMBeanInvocation";
   /** A serialized MarshalledValue */
   private static String RESPONSE_CONTENT_TYPE =
   "application/x-java-serialized-object; class=org.jboss.invocation.MarshalledValue";
   private javax.management.MBeanServer mbeanServer;
   
   /** Initializes the servlet.
    */
   public void init (javax.servlet.ServletConfig config) throws ServletException
   {
      super.init (config);
      
      // Lookup the MBeanServer
      mbeanServer = org.jboss.mx.util.MBeanServerLocator.locateJBoss();
      if( mbeanServer == null )
         throw new ServletException ("Failed to locate the MBeanServer");
   }
   
   /** Destroys the servlet.
    */
   public void destroy ()
   {
      
   }
   
   /** Read a MarshalledInvocation and dispatch it to the target JMX object
    * invoke(Invocation) object.
    *
    * @param request servlet request
    * @param response servlet response
    */
   protected void processRequest (javax.servlet.http.HttpServletRequest request, javax.servlet.http.HttpServletResponse response) throws ServletException, java.io.IOException
   {
      boolean trace = log.isTraceEnabled ();
      if( trace )
      {
         log.trace ("processRequest, ContentLength: "+request.getContentLength ());
         log.trace ("processRequest, ContentType: "+request.getContentType ());
      }
      
      try
      {
         response.setContentType (RESPONSE_CONTENT_TYPE);
         // See if the request already has the MarshalledInvocation
         Object mi = request.getAttribute ("RemoteMBeanInvocation");
         if( mi == null )
         {
            // Get the invocation from the post
            javax.servlet.ServletInputStream sis = request.getInputStream ();
            java.io.ObjectInputStream ois = new java.io.ObjectInputStream (sis);
            mi = ois.readObject ();
            ois.close ();
         }
         
         // Forward the invocation onto the JMX bus
         Object value = null;
         if (mi instanceof RemoteMBeanInvocation)
         {
            RemoteMBeanInvocation invocation = (RemoteMBeanInvocation)mi;
            value = mbeanServer.invoke (invocation.targetObjectName, invocation.actionName, invocation.params, invocation.signature);
         }
         else
         {
            RemoteMBeanAttributeInvocation invocation = (RemoteMBeanAttributeInvocation)mi;
            value = mbeanServer.getAttribute(invocation.targetObjectName, invocation.attributeName);
         }
         org.jboss.invocation.MarshalledValue mv = new org.jboss.invocation.MarshalledValue (value);
         javax.servlet.ServletOutputStream sos = response.getOutputStream ();
         java.io.ObjectOutputStream oos = new java.io.ObjectOutputStream (sos);
         oos.writeObject (mv);
         oos.close ();
      }
      catch(Throwable t)
      {
         t = org.jboss.mx.util.JMXExceptionDecoder.decode (t);
         org.jboss.invocation.InvocationException appException = new org.jboss.invocation.InvocationException (t);
         log.debug ("Invoke threw exception", t);
         // Marshall the exception
         response.resetBuffer ();
         org.jboss.invocation.MarshalledValue mv = new org.jboss.invocation.MarshalledValue (appException);
         javax.servlet.ServletOutputStream sos = response.getOutputStream ();
         java.io.ObjectOutputStream oos = new java.io.ObjectOutputStream (sos);
         oos.writeObject (mv);
         oos.close ();
      }
   }
   
   /** Handles the HTTP <code>GET</code> method.
    * @param request servlet request
    * @param response servlet response
    */
   protected void doGet (javax.servlet.http.HttpServletRequest request, javax.servlet.http.HttpServletResponse response) throws ServletException, java.io.IOException
   {
      processRequest (request, response);
   }
   
   /** Handles the HTTP <code>POST</code> method.
    * @param request servlet request
    * @param response servlet response
    */
   protected void doPost (javax.servlet.http.HttpServletRequest request, javax.servlet.http.HttpServletResponse response) throws ServletException, java.io.IOException
   {
      processRequest (request, response);
   }
   
   /** Returns a short description of the servlet.
    */
   public String getServletInfo ()
   {
      return "An HTTP to JMX MBeanServer servlet";
   }
   
}
