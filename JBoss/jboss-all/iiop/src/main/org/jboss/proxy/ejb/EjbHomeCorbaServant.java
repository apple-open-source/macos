/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.proxy.ejb;

import java.security.Principal;
import java.util.Map;
import javax.ejb.HomeHandle;
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
import org.omg.PortableServer.POA;

import org.jboss.iiop.rmi.marshal.strategy.SkeletonStrategy;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationContext;
import org.jboss.invocation.InvocationKey;
import org.jboss.invocation.InvocationType;
import org.jboss.invocation.PayloadKey;
import org.jboss.invocation.iiop.ServantWithMBeanServer;
import org.jboss.logging.Logger;

/**
 * CORBA servant class for an <code>EJBHome</code>. An instance of this class 
 * "implements" a single <code>EJBHome</code> by forwarding to the bean 
 * container all IIOP invocations on the bean home. Such invocations are routed
 * through the JBoss <code>MBean</code> server, which delivers them to the 
 * target container. 
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.4.2.1 $
 */
public class EjbHomeCorbaServant 
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
    * Mapping from home methods to <code>SkeletonStrategy</code> instances.
    */
   private final Map methodInvokerMap;

   /**
    * CORBA repository ids of the RMI-IDL interfaces implemented by the bean's
    * home (<code>EJBHome</code> instance).
    */
   private final String[] repositoryIds;

   /**
    * CORBA reference to an IR object representing the bean's home interface.
    */
   private final InterfaceDef interfaceDef;

   /**
    * This servant's logger.
    */ 
   private final Logger logger;

   /**
    * <code>HomeHandle</code> for the <code>EJBHome</code> 
    * implemented by this servant.
    */
   private HomeHandle homeHandle = null;

   /**
    * A reference to the JBoss <code>MBean</code> server.
    */
   private MBeanServer mbeanServer;

   /**
    * Constructs an <code>EjbHomeCorbaServant></code>.
    */ 
   public EjbHomeCorbaServant(ObjectName containerName,
                              ClassLoader containerClassLoader,
                              Map methodInvokerMap,
                              String[] repositoryIds,
                              InterfaceDef interfaceDef,
                              Logger logger)
   {
      this.containerName = containerName;
      this.containerClassLoader = containerClassLoader;
      this.methodInvokerMap = methodInvokerMap;
      this.repositoryIds = repositoryIds;
      this.interfaceDef = interfaceDef;
      this.logger = logger;
   }

   public void setHomeHandle(HomeHandle homeHandle)
   {
      this.homeHandle = homeHandle;
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
    * Returns an IR object describing the bean's home interface.
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
    * Returns an array with the CORBA repository ids of the RMI-IDL 
    * interfaces implemented by the container's <code>EJBHome</code>.
    */
   public String[] _all_interfaces(POA poa, byte[] objectId) 
   {
      return (String[])repositoryIds.clone();
   }
   
   /**
    * Receives IIOP requests to an <code>EJBHome</code> and forwards them to 
    * its container, through the JBoss <code>MBean</code> server.
    */
   public OutputStream _invoke(String opName,
                               InputStream in,
                               ResponseHandler handler) 
   {
      if (logger.isTraceEnabled()) {
         logger.trace("EJBHome invocation: " + opName);
      }
      
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(containerClassLoader);
      
      try {
         
         SkeletonStrategy op = (SkeletonStrategy) methodInvokerMap.get(opName);
         if (op == null) {
            throw new BAD_OPERATION(opName);
         }

         org.omg.CORBA_2_3.portable.OutputStream out;
         try {
            Object retVal;
            
            // The EJBHome method getHomeHandle() receives special 
            // treatment because the container does not implement it. 
            // The remaining EJBObject methods (getEJBMetaData, 
            // remove(java.lang.Object), and remove(javax.ejb.Handle))
            // are forwarded to the container.
            
            if (opName.equals("_get_homeHandle")) {
               retVal = homeHandle;
            }
            else {
               Object[] params = op.readParams(
                                  (org.omg.CORBA_2_3.portable.InputStream)in);
               Invocation inv = new Invocation(null, 
                                               op.getMethod(), 
                                               params,
                                               null, /* tx */
                                               null, /* identity */
                                               null  /* credential*/);
               inv.setValue(InvocationKey.INVOKER_PROXY_BINDING, 
                            "iiop", 
                            PayloadKey.AS_IS);
               inv.setType(InvocationType.HOME);
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
               logger.trace("Exception in EJBHome invocation", e);
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
   
   // Implementation of the interface LocalIIOPInvoker ---------------------
   
   /**
    * Receives intra-VM requests to an <code>EJBHome</code> and forwards them 
    * to its container (through the JBoss <code>MBean</code> server).
    */
   public Object invoke(String opName,
                        Object[] arguments, 
                        Transaction tx, 
                        Principal identity, 
                        Object credential)
      throws Exception
   {
      if (logger.isTraceEnabled()) {
         logger.trace("EJBHome local invocation: " + opName);
      }
      
      ClassLoader oldCl = Thread.currentThread().getContextClassLoader();
      Thread.currentThread().setContextClassLoader(containerClassLoader);
      
      try {
         SkeletonStrategy op = 
            (SkeletonStrategy) methodInvokerMap.get(opName);
         if (op == null) {
            throw new BAD_OPERATION(opName);
         }
         
         Invocation inv = new Invocation(null, 
                                         op.getMethod(), 
                                         arguments,
                                         null, /* tx */
                                         null, /* identity */
                                         null  /* credential */);
         inv.setValue(InvocationKey.INVOKER_PROXY_BINDING, 
                      "iiop", 
                      PayloadKey.AS_IS);
         inv.setType(InvocationType.HOME);
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
