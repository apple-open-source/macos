/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
 
package org.jboss.iiop;

import java.io.InputStream;
import java.util.Hashtable;
import java.util.Properties;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.Name;
import javax.naming.Reference;
import javax.naming.spi.ObjectFactory;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.server.ServerConfigUtil;
import org.omg.CORBA.ORB;
import org.omg.CORBA.Policy;
import org.omg.PortableServer.IdAssignmentPolicy;
import org.omg.PortableServer.IdAssignmentPolicyValue;
import org.omg.PortableServer.LifespanPolicy;
import org.omg.PortableServer.LifespanPolicyValue;
import org.omg.PortableServer.POA;
import org.omg.PortableServer.POAHelper;

/**
 *  This is a JMX service that provides the default CORBA ORB
 *  for JBoss to use.
 *      
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @author <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 *  @version $Revision: 1.20.2.5 $
 */
public class CorbaORBService
      extends ServiceMBeanSupport
      implements CorbaORBServiceMBean, ObjectFactory
{
   // Constants -----------------------------------------------------
   public static String ORB_NAME = "JBossCorbaORB";
   public static String POA_NAME = "JBossCorbaPOA";
   public static String IR_POA_NAME = "JBossCorbaInterfaceRepositoryPOA";
    
   // Attributes ----------------------------------------------------

   private String orbClass = null;
   private String orbSingletonClass = null;
   private String orbSingletonDelegate = null;
   private String orbPropertiesFileName = "orb-properties-file-not-defined";
   private String portableInterceptorInitializerClass = null;
   private Integer port = null;

   // Static --------------------------------------------------------

   private static ORB orb;
   private static POA poa;
   private static POA irPoa;

   // ServiceMBeanSupport overrides ---------------------------------

   protected void createService() 
      throws Exception 
   {

      Properties props = new Properties();

      // Read orb properties file into props
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      InputStream is = cl.getResourceAsStream(orbPropertiesFileName);
      props.load(is);
      String oaiAddr = props.getProperty("OAIAddr");
      if (oaiAddr == null)
         oaiAddr = ServerConfigUtil.getSpecificBindAddress();
      if (oaiAddr != null)
         props.setProperty("OAIAddr", oaiAddr);
      log.info("Using OAIAddr=" + oaiAddr);

      // Initialize the ORB
      Properties systemProps = System.getProperties();
      if (orbClass != null) {
         props.put("org.omg.CORBA.ORBClass", orbClass);
         systemProps.put("org.omg.CORBA.ORBClass", orbClass);
      }
      if (orbSingletonClass != null) {
         props.put("org.omg.CORBA.ORBSingletonClass", orbSingletonClass);
         systemProps.put("org.omg.CORBA.ORBSingletonClass", orbSingletonClass);
      }
      if (orbSingletonDelegate != null)
         systemProps.put(org.jboss.system.ORBSingleton.DELEGATE_CLASS_KEY,
                         orbSingletonDelegate);
      String jacorbVerbosity = props.getProperty("jacorb.verbosity");
      // JacORB-specific hack: we add jacorb.verbosity to the system properties
      // just to avoid the warning "jacorb.properties not found".
      if (jacorbVerbosity != null)
         systemProps.put("jacorb.verbosity", jacorbVerbosity);
      System.setProperties(systemProps);
      if (portableInterceptorInitializerClass != null)
         props.put("org.omg.PortableInterceptor.ORBInitializerClass."
                   + portableInterceptorInitializerClass, "");

      // Allows overriding of OAPort from config file through MBean config
      if (this.port != null)
         props.put("OAPort", this.port.toString());

      orb = ORB.init(new String[0], props);
      bind(ORB_NAME, "org.omg.CORBA.ORB");
      CorbaORB.setInstance(orb);

      // Initialize the POA
      poa = POAHelper.narrow(orb.resolve_initial_references("RootPOA"));
      poa.the_POAManager().activate();
      bind(POA_NAME, "org.omg.PortableServer.POA");

      // Make the ORB work
      new Thread(
         new Runnable() {
            public void run() {
               orb.run();
            }
         }, "ORB thread"
      ).start(); 

      // Create a POA for interface repositories
      try {
         LifespanPolicy lifespanPolicy = 
            poa.create_lifespan_policy(LifespanPolicyValue.PERSISTENT);
         IdAssignmentPolicy idAssignmentPolicy = 
            poa.create_id_assignment_policy(IdAssignmentPolicyValue.USER_ID);

         irPoa = poa.create_POA("IR", null,
                                new Policy[]{lifespanPolicy, 
                                             idAssignmentPolicy});
         bind(IR_POA_NAME, "org.omg.PortableServer.POA");
         
         // Activate the poa
         irPoa.the_POAManager().activate();
         
      } catch (Exception ex) {
         getLog().error("Error in IR POA initialization", ex);
      }

//    // Create an interface repository just for testing (TODO: remove this)
//    try {
//       org.jboss.iiop.rmi.ir.InterfaceRepository iri = 
//          new org.jboss.iiop.rmi.ir.InterfaceRepository(orb, irPoa, "IR");
// 
//       // Test this interface repository
//       iri.mapClass(org.jboss.iiop.test.TestBase.class);
//       iri.mapClass(org.jboss.iiop.test.Test.class);
//       iri.mapClass(org.jboss.iiop.test.TestValue.class);
//       iri.mapClass(org.jboss.iiop.test.TestException.class);
//       iri.finishBuild();
//       
//       java.io.FileOutputStream fos = new java.io.FileOutputStream("ir.ior");
//       java.io.OutputStreamWriter osw = new java.io.OutputStreamWriter(fos);
//       osw.write(iri.getReference());
//       osw.flush();
//       fos.flush();
//       osw.close();
//       fos.close();
//    } catch (org.jboss.iiop.rmi.RMIIIOPViolationException violation) {
//          getLog().error("RMI/IIOP violation, section: " 
//                         + violation.getSection(), violation);
//    } catch (Exception ex) {
//          getLog().error("Error in interface repository initialization", ex);
//    }

      /* Create the JSR77 Managed Object
      mJSR77 = RMI_IIOPResource.create(
         getServer(),
         ORB_NAME,
         getServiceName()
      );
      */
   }

   protected void destroyService() throws Exception
   {
      /*
      if( mJSR77 != null )
      {
          RMI_IIOPResource.destroy(
             getServer(),
             ORB_NAME
          );
          mJSR77 = null;
      }
      */
      try {
         // Unbind from JNDI
         unbind(ORB_NAME);
         unbind(POA_NAME);
         unbind(IR_POA_NAME);

         // Stop ORB
         orb.shutdown(false);
      } catch (Exception e) {
         log.error("Exception while stopping ORB service", e);
      }
   }

   // CorbaORBServiceMBean implementation ---------------------------

   public String getORBClass()
   {
      return orbClass;
   }

   public void setORBClass(String orbClass)
   {
      this.orbClass = orbClass;
   }

   public String getORBSingletonClass()
   {
      return orbSingletonClass;
   }

   public void setORBSingletonClass(String orbSingletonClass)
   {
      this.orbSingletonClass = orbSingletonClass;
   }

   public String getORBSingletonDelegate()
   {
      return orbSingletonDelegate;
   }

   public void setORBSingletonDelegate(String orbSingletonDelegate)
   {
      this.orbSingletonDelegate = orbSingletonDelegate;
   }

   public void setORBPropertiesFileName(String orbPropertiesFileName)
   {
      this.orbPropertiesFileName = orbPropertiesFileName;
   }

   public String getORBPropertiesFileName()
   {
      return orbPropertiesFileName;
   }

   public String getPortableInterceptorInitializerClass()
   {
      return portableInterceptorInitializerClass;
   }

   public void setPortableInterceptorInitializerClass(
                                 String portableInterceptorInitializerClass)
   {
      this.portableInterceptorInitializerClass =
                                          portableInterceptorInitializerClass;
   }

   public void setPort(int port)
   {
      this.port = new Integer(port);
   }

   public int getPort()
   {
      return this.port.intValue();
   }

   // ObjectFactory implementation ----------------------------------

   public Object getObjectInstance(Object obj, Name name,
                                   Context nameCtx, Hashtable environment)
      throws Exception
   {
      String s = name.toString();
      if (getLog().isTraceEnabled())
         getLog().trace("getObjectInstance: obj.getClass().getName=\"" +
                        obj.getClass().getName() +
                        "\n                   name=" + s);
      if (ORB_NAME.equals(s))
         return orb;
      if (POA_NAME.equals(s))
         return poa;
      if (IR_POA_NAME.equals(s))
         return irPoa;
      return null;
   }


   // Private -------------------------------------------------------

   private void bind(String name, String className)
      throws Exception
   {
      Reference ref = new Reference(className, getClass().getName(), null);
      new InitialContext().bind("java:/"+name, ref);
   }

   private void unbind(String name)
      throws Exception
   {
      new InitialContext().unbind("java:/"+name);
   }

}
