/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.proxy.ejb;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.security.Principal;
import java.util.Map;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.transaction.Transaction;
import org.omg.CORBA.BAD_OPERATION;
import org.omg.CORBA.InterfaceDef;
import org.omg.CORBA.portable.InvokeHandler;
import org.omg.CORBA.portable.InputStream;
import org.omg.CORBA.portable.OutputStream;
import org.omg.CORBA.portable.ResponseHandler;
import org.omg.CORBA.portable.UnknownException;
import org.omg.PortableServer.Current;
import org.omg.PortableServer.POA;

import org.jboss.iiop.rmi.marshal.strategy.SkeletonStrategy;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationContext;
import org.jboss.invocation.InvocationKey;
import org.jboss.invocation.InvocationType;
import org.jboss.invocation.PayloadKey;
import org.jboss.invocation.iiop.ReferenceData;
import org.jboss.invocation.iiop.ServantWithMBeanServer;
import org.jboss.logging.Logger;

/**
 * CORBA servant class for the <code>EJBObject</code>s of a given bean. An 
 * instance of this class "implements" the bean's set of <code>EJBObject</code>
 * instances by forwarding to the bean container all IIOP invocations on any
 * of the bean's <code>EJBObject</code>s. Such invocations are routed through 
 * the JBoss <code>MBean</code> server, which delivers them to the target 
 * container. 
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.4.2.1 $
 */
public class EjbObjectCorbaServant 
      extends ServantWithMBeanServer
      implements InvokeHandler, LocalIIOPInvoker {

   /**
    * The <code>MBean</code> name of this servant's container.
    */
   private final ObjectName containerName;

   /**
    * The classloader of this servant's container.
    */
   private final ClassLoader containerClassLoader;

   /**
    * Thread-local <code>Current</code> object from which we get the target oid
    * in an incoming IIOP request.
    */
   private final Current poaCurrent;

   /**
    * Mapping from bean methods to <code>SkeletonStrategy</code> instances.
    */
   private final Map methodInvokerMap;

   /**
    * CORBA repository ids of the RMI-IDL interfaces implemented by the bean 
    * (<code>EJBObject</code> instance).
    */
   private final String[] repositoryIds;

   /**
    * CORBA reference to an IR object representing the bean's remote interface.
    */
   private final InterfaceDef interfaceDef;

   /**
    * This servant's logger.
    */ 
   private final Logger logger;

   /**
    * A reference to the JBoss <code>MBean</code> server.
    */
   private MBeanServer mbeanServer;

   /**
    * Constructs an <code>EjbObjectCorbaServant></code>.
    */ 
   public EjbObjectCorbaServant(ObjectName containerName,
                                ClassLoader containerClassLoader,
                                Current poaCurrent,
                                Map methodInvokerMap,
                                String[] repositoryIds,
                                InterfaceDef interfaceDef,
                                Logger logger)
   {
      this.containerName = containerName;
      this.containerClassLoader = containerClassLoader;
      this.poaCurrent = poaCurrent;
      this.methodInvokerMap = methodInvokerMap;
      this.repositoryIds = repositoryIds;
      this.interfaceDef = interfaceDef;
      this.logger = logger;
   }

   // Implementation of method declared as abstract in the superclass ------

   /**
    * Sets this servant's <code>MBeanServer</code>.
    */
   public void setMBeanServer(MBeanServer mbeanServer)
   {
      this.mbeanServer = mbeanServer;
   }
   
   // This method overrides the one in org.omg.PortableServer.Servant ------

   /**
    * Returns an IR object describing the bean's remote interface.
    */
   public org.omg.CORBA.Object _get_interface_def()
   {
      if (interfaceDef != null)
         return interfaceDef;
      else
         return super._get_interface_def();
   }
   
   // Implementation of org.omg.CORBA.portable.InvokeHandler ---------------

   /**
    * Returns an array with the CORBA repository ids of the RMI-IDL interfaces 
    * implemented by this servant's <code>EJBObject</code>s.
    */
   public String[] _all_interfaces(POA poa, byte[] objectId) 
   {
      return (String[])repositoryIds.clone();
   }

   /**
    * Receives IIOP requests to this servant's <code>EJBObject</code>s
    * and forwards them to the bean container, through the JBoss 
    * <code>MBean</code> server. 
    */
   public OutputStream _invoke(String opName,
                               InputStream in,
                               ResponseHandler handler) 
   {

      if (logger.isTraceEnabled()) {
         logger.trace("EJBObject invocation: " + opName);
      }

      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(containerClassLoader);
      
      try {
         SkeletonStrategy op = (SkeletonStrategy) methodInvokerMap.get(opName);
         if (op == null) {
            throw new BAD_OPERATION(opName);
         }

         Object id;
         try {
            id = ReferenceData.extractObjectId(poaCurrent.get_object_id());
            if (logger.isTraceEnabled() && id != null) 
               logger.trace("                      id class is " 
                            + id.getClass().getName());
         }
         catch (Exception e) {
            logger.error("Error getting EJBObject id", e);
            throw new UnknownException(e);
         }
         
         org.omg.CORBA_2_3.portable.OutputStream out;
         try {
            Object retVal;
            
            // The EJBObject methods getHandle() and getPrimaryKey() receive
            // special treatment because the container does not implement 
            // them. The remaining EJBObject methods (getEJBHome(), remove(),
            // and isIdentical()) are forwarded to the container.

            if (opName.equals("_get_primaryKey")) {
               retVal = id;
            }
            else if (opName.equals("_get_handle")) {
               retVal = new HandleImplIIOP(_this_object());
            }
            else {
               Object[] params = 
                  op.readParams((org.omg.CORBA_2_3.portable.InputStream)in);
               Invocation inv = new Invocation(id, 
                                               op.getMethod(), 
                                               params,
                                               null, /* tx */
                                               null, /* identity */
                                               null  /* credential */);
               inv.setValue(InvocationKey.INVOKER_PROXY_BINDING, 
                            "iiop", 
                            PayloadKey.AS_IS);
               inv.setType(InvocationType.REMOTE);
               retVal = mbeanServer.invoke(containerName,
                                           "invoke",
                                           new Object[] {inv},
                                           Invocation.INVOKE_SIGNATURE);

            }
            out = (org.omg.CORBA_2_3.portable.OutputStream) 
                  handler.createReply();
            if (op.isNonVoid()) {
               op.writeRetval(out, retVal);
            }
         }
         catch (Exception e) {
            if (logger.isTraceEnabled()) {
               logger.trace("Exception in EJBObject invocation", e);
            }
            if (e instanceof MBeanException) {
               e = ((MBeanException)e).getTargetException();
            }
            out = (org.omg.CORBA_2_3.portable.OutputStream) 
                  handler.createExceptionReply();
            op.writeException(out, e);
         }
         return out;
      }
      finally {
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }
   
   // Implementation of the interface LocalIIOPInvoker ------------------------

   /**
    * Receives intra-VM invocations on this servant's <code>EJBObject</code>s
    * and forwards them to the bean container, through the JBoss 
    * <code>MBean</code> 
    * server. 
    */
   public Object invoke(String opName,
                 Object[] arguments, 
                 Transaction tx, 
                 Principal identity, 
                 Object credential)
      throws Exception
   {
      if (logger.isTraceEnabled()) {
         logger.trace("EJBObject local invocation: " + opName);
      }

      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(containerClassLoader);

      try {
         SkeletonStrategy op = (SkeletonStrategy) methodInvokerMap.get(opName);
         if (op == null) {
            throw new BAD_OPERATION(opName);
         }
         
         Object id;
         try {
            id = ReferenceData.extractObjectId(poaCurrent.get_object_id());
            if (logger.isTraceEnabled() && id != null) {
               logger.trace("                      id class is " 
                            + id.getClass().getName());
            }
         }
         catch (Exception e) {
            logger.error("Error getting EJBObject id", e);
            throw new UnknownException(e);
         }
         
         Invocation inv = new Invocation(id, 
                                         op.getMethod(), 
                                         arguments,
                                         null, /* tx */
                                         null, /* identity */
                                         null  /* credential */);
         inv.setValue(InvocationKey.INVOKER_PROXY_BINDING, 
                      "iiop", 
                      PayloadKey.AS_IS);
         inv.setType(InvocationType.REMOTE);
         return mbeanServer.invoke(containerName,
                                   "invoke",
                                   new Object[] {inv},
                                   Invocation.INVOKE_SIGNATURE);
      }
      catch (MBeanException e) {
         throw e.getTargetException();
      }
      finally {
         Thread.currentThread().setContextClassLoader(oldCl);
      }
   }

}
