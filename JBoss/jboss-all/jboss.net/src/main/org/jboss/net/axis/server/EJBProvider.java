/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: EJBProvider.java,v 1.3.4.4 2003/03/28 12:50:46 cgjung Exp $

package org.jboss.net.axis.server;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Iterator;

import javax.naming.NamingException;
import javax.servlet.http.HttpSessionBindingEvent;
import javax.servlet.http.HttpSessionBindingListener;
import javax.xml.namespace.QName;
import javax.xml.rpc.server.ServiceLifecycle;

import org.apache.axis.AxisFault;
import org.apache.axis.EngineConfiguration;
import org.apache.axis.MessageContext;
import org.apache.axis.description.OperationDesc;
import org.apache.axis.description.ServiceDesc;
import org.apache.axis.handlers.soap.SOAPService;
import org.apache.axis.message.SOAPEnvelope;

import org.jboss.net.axis.XMLResourceProvider;

/**
 * <p>
 * A JBoss-compatible EJB Provider that exposes the methods of
 * any session bean as a web service endpoint. 
 * </p>
 * <p>
 * Basically it is a slimmed downed derivative of 
 * the Axis-EJBProvider without the usual, corba-related configuration mumbo-jumbo
 * that is operating under the presumption that the right classloader has already been set
 * by the request flow chain (@see org.jboss.net.axis.SetClassLoaderHandler).
 * </p>
 * <p>
 * Since Version 1.5 and thanks to Kevin Conner, we now also support 
 * stateful beans that are tied to the service scope (you should reasonably 
 * choose scope="session" in the <service/> tag of the corresponding web-service.xml) 
 * Note that by using a jaxp lifecycle handler we synchronize with 
 * web service scopes that can be shorter-lived than the surrounding http-session. 
 * However, as I understood Kevin and from my observations, it seems that Axis 
 * and hence the jaxp lifecycle does not get notified upon http-session expiration. 
 * Hence our lifecycle listener currently implements the http session 
 * lifecycle, too (which is not very transport-agnostic, but who cares 
 * at the moment).
 * </p>  
 * <p>
 * EJBProvider is able to recognize an {@link WsdlAwareHttpActionHandler} in its
 * transport chain such that it will set the soap-action headers in the wsdl.
 * </p>
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @since 5. Oktober 2001, 13:02
 * @version $Revision: 1.3.4.4 $
 */

public class EJBProvider extends org.apache.axis.providers.java.EJBProvider {

   //
   // Attributes
   //

   /** the real remote class we are shielding */
   protected Class remoteClass;

   /** we are caching the home for perfomance purposes */
   protected Object ejbHome;

   /** we are caching the create method for perfomance purposes */
   protected Method ejbCreateMethod;

   //
   // Constructors
   //

   /** Creates new EJBProvider */
   public EJBProvider() {
   }

   //
   // Helper Methods
   //

   /**
    * access home factory via jndi if 
    * reference is not yet cached
    */

   protected synchronized Object getEJBHome(String jndiName)
      throws NamingException {
      if (ejbHome == null) {
         // Get the EJB Home object from JNDI
         ejbHome = this.getCachedContext().lookup(jndiName);
      }
      return ejbHome;
   }

   /**
    * access home factory via jndi if 
    * reference is not yet cached
    */

   protected synchronized Method getEJBCreateMethod(String jndiName)
      throws NamingException, NoSuchMethodException {
      if (ejbCreateMethod == null) {
         Object ejbHome = getEJBHome(jndiName);
         ejbCreateMethod =
            ejbHome.getClass().getMethod("create", empty_class_array);
      }

      return ejbCreateMethod;
   }

   /**
    * Return the object which implements the service lifecycle. Makes the usual
    * lookup->create call w/o the PortableRemoteDaDaDa for the sake of Corba.
    * @param msgContext the message context
    * @param clsName The JNDI name of the EJB home class
    * @return an object that implements the service
    */
   protected Object makeNewServiceObject(
      MessageContext msgContext,
      String clsName)
      throws Exception {

      // abuse the clsName as jndi lookup name
      Object ejbHome = getEJBHome(clsName);
      // Invoke the create method of the ejbHome class without actually
      // touching any EJB classes (i.e. no cast to EJBHome)
      Method createMethod = getEJBCreateMethod(clsName);
      // shield behind the lifecycle service
      return new EJBServiceLifeCycle(
         createMethod.invoke(ejbHome, empty_object_array));
   }

   /**
    * Return the class name of the service, note that this could be called
    * outside the correct chain, e.g., by the url mapper. Hence we need
    * to find the right classloader.
    */

   protected synchronized Class getServiceClass(
      String beanJndiName,
      SOAPService service,
      MessageContext msgContext)
      throws AxisFault {
      if (remoteClass == null) {

         // we need to find the right service classloader first
         // through the engine
         EngineConfiguration engineConfig =
            msgContext != null ? msgContext.getAxisEngine().getConfig() : null;

         ClassLoader currentLoader =
            Thread.currentThread().getContextClassLoader();

         if (engineConfig != null
            && engineConfig instanceof XMLResourceProvider) {
            XMLResourceProvider config = (XMLResourceProvider) engineConfig;

            ClassLoader newLoader =
               config.getMyDeployment().getClassLoader(
                  new QName(null, service.getName()));

            // enter the classloader
            Thread.currentThread().setContextClassLoader(newLoader);
         }

         try {
            remoteClass = getEJBCreateMethod(beanJndiName).getReturnType();
         } catch (NamingException e) {
            throw new AxisFault("Could not find home in JNDI", e);
         } catch (NoSuchMethodException e) {
            throw new AxisFault("Could not find create method at home ;-)", e);
         } finally {
            Thread.currentThread().setContextClassLoader(currentLoader);
         }

      }

      return remoteClass;
   }

   //
   // Public API
   //

   /**
    * Generate the WSDL for this service.
    * We need to rearrange the classloader stuff for that purpose similar
    * as we did with the service class interface
    */

   public void generateWSDL(MessageContext msgContext) throws AxisFault {
      EngineConfiguration engineConfig =
         msgContext != null ? msgContext.getAxisEngine().getConfig() : null;

      ClassLoader currentLoader =
         Thread.currentThread().getContextClassLoader();

      if (engineConfig != null
         && engineConfig instanceof XMLResourceProvider) {
         XMLResourceProvider config = (XMLResourceProvider) engineConfig;
         ClassLoader newLoader =
            config.getMyDeployment().getClassLoader(
               new QName(null, msgContext.getTargetService()));
         Thread.currentThread().setContextClassLoader(newLoader);
      }

      try {

         // check whether there is an http action header present
         if (msgContext != null) {

            boolean isSoapAction =
               msgContext.getProperty(
                  Constants.ACTION_HANDLER_PRESENT_PROPERTY)
                  == Boolean.TRUE;

            // yes, then loop through the operation descriptions
            for (Iterator alloperations =
               msgContext
                  .getService()
                  .getServiceDescription()
                  .getOperations()
                  .iterator();
               alloperations.hasNext();
               ) {
               OperationDesc opDesc = (OperationDesc) alloperations.next();
               // and add a soap action tag with the name of the service
               opDesc.setSoapAction(
                  isSoapAction ? msgContext.getService().getName() : null);
            }
         }

         super.generateWSDL(msgContext);

      } finally {
         Thread.currentThread().setContextClassLoader(currentLoader);
      }
   }

   /**
    * Override processMessage of super class in order
    * to unpack the service object from the lifecycle
    */
   public void processMessage(
      MessageContext msgContext,
      SOAPEnvelope reqEnv,
      SOAPEnvelope resEnv,
      Object obj)
      throws Exception {
      super.processMessage(
         msgContext,
         reqEnv,
         resEnv,
         ((EJBServiceLifeCycle) obj).serviceObject);
   }

   /*
    * Adds the correct stop classes and soap-action annotations to wsdl generation 
    * @see org.apache.axis.providers.java.EJBProvider#initServiceDesc(org.apache.axis.handlers.soap.SOAPService,org.apache.axis.MessageContext)
    */
   public void initServiceDesc(SOAPService service, MessageContext msgContext)
      throws AxisFault {
      // the service class used to fill service description is the EJB Remote/Local Interface
      // we add EJBObject and EJBLocalObject as stop classes because we		 
      // don't want any of their methods in the wsdl ...
      ServiceDesc serviceDescription = service.getServiceDescription();
      ArrayList stopClasses = serviceDescription.getStopClasses();
      if (stopClasses == null)
         stopClasses = new ArrayList();
      stopClasses.add("javax.ejb.EJBObject");
      stopClasses.add("javax.ejb.EJBLocalObject");
      serviceDescription.setStopClasses(stopClasses);
      // once the stop classes are right, we can generate meta-data for only
      // the wanted methods
      super.initServiceDesc(service, msgContext);
   }

   //
   // Inner Classes
   //

   /**
    * This is the lifecycle object that is registered in the 
    * message scope and that shields the proper bean reference
    */

   protected static class EJBServiceLifeCycle
      implements ServiceLifecycle, HttpSessionBindingListener {

      //
      // Attributes
      //

      /** may be local or remote object */
      protected Object serviceObject;

      //
      // Constructors
      //

      /** constructs a new lifecycle */
      protected EJBServiceLifeCycle(Object serviceObject) {
         this.serviceObject = serviceObject;
      }

      //
      // Public API
      //

      /**
       * call remove 
       * @see javax.xml.rpc.server.ServiceLifecycle#destroy()
       */
      public void destroy() {
         try {
            if (serviceObject instanceof javax.ejb.EJBObject) {
               try {
                  ((javax.ejb.EJBObject) serviceObject).remove();
               } catch (java.rmi.RemoteException e) {
                  // have yet to decide what to do
               }
            } else {
               ((javax.ejb.EJBLocalObject) serviceObject).remove();
            }
         } catch (javax.ejb.RemoveException e) {
            // have yet to decide what to do
         }
      }

      /**
       * Nothing to be done
       * @see javax.xml.rpc.server.ServiceLifecycle#init(Object)
       */
      public void init(Object arg0) {
      }

      /**
       * @see javax.servlet.http.HttpSessionBindingListener#valueBound(HttpSessionBindingEvent)
       */
      public void valueBound(HttpSessionBindingEvent arg0) {
         init(arg0);
      }

      /**
       * @see javax.servlet.http.HttpSessionBindingListener#valueUnbound(HttpSessionBindingEvent)
       */
      public void valueUnbound(HttpSessionBindingEvent arg0) {
         destroy();
      }

   } // EJBServiceLifeCycle

} // EJBProvider
