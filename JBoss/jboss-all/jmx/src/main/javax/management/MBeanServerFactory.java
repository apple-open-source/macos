/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.util.Map;
import java.util.List;
import java.util.HashMap;
import java.util.ArrayList;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;

import javax.management.loading.ClassLoaderRepository;
import javax.management.loading.DefaultLoaderRepository;

import org.jboss.mx.server.ServerConstants;

/**
 * MBeanServerFactory is used to create instances of MBean servers.
 *
 * @see javax.management.MBeanServer
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.4.8.1 $
 *   
 */
public class MBeanServerFactory
{
   private static final MBeanServerPermission CREATE = 
      new MBeanServerPermission("createMBeanServer", null);
   private static final MBeanServerPermission FIND = 
      new MBeanServerPermission("findMBeanServer", null);
   private static final MBeanServerPermission NEW = 
      new MBeanServerPermission("newMBeanServer", null);
   private static final MBeanServerPermission RELEASE = 
      new MBeanServerPermission("releaseMBeanServer", null);

   // Attributes ----------------------------------------------------
   private static Map serverMap = new HashMap();

   // Public --------------------------------------------------------
   public static void releaseMBeanServer(MBeanServer mbeanServer)
   {
      checkPermission(RELEASE);
      try
      {
         String agentID = (String)mbeanServer.getAttribute(
               new ObjectName(ServerConstants.MBEAN_SERVER_DELEGATE),
               "MBeanServerId"
         );

         Object server = serverMap.remove(agentID);

         if (server == null)
            throw new IllegalArgumentException("MBean server reference not found.");
      }
      catch (MalformedObjectNameException e)
      {
         throw new Error(e.toString());
      }
      catch (JMException e)
      {
         throw new Error("Cannot retrieve AgentID: " + e.toString());
      }
   }

   public static MBeanServer createMBeanServer()
   {
      checkPermission(CREATE);
      return createMBeanServer(ServerConstants.DEFAULT_DOMAIN);
   }

   public static MBeanServer createMBeanServer(String domain)
   {
      checkPermission(CREATE);
      return createMBeanServer(domain, true);
   }

   public static MBeanServer newMBeanServer()
   {
      checkPermission(NEW);
      return newMBeanServer(ServerConstants.DEFAULT_DOMAIN);
   }

   public static MBeanServer newMBeanServer(String domain)
   {
      checkPermission(NEW);
      return createMBeanServer(domain, false);
   }

   public static ArrayList findMBeanServer(String agentId)
   {
      checkPermission(FIND);
      if (agentId != null)
      {
         ArrayList list = new ArrayList(1);
         Object server = serverMap.get(agentId);
         
         if (server != null)
            list.add(server);
            
         return list;
      }

      return new ArrayList(serverMap.values());
   }

   /**
    * Returns the classloader repository for an MBeanServer
    *
    * @todo implement this, currently it just returns a wrapper for the default
    *       loader repository
    *
    * @param server the mbean server, pass null for the default loader repository
    *        shared by all mbeanservers
    * @return the loader repository
    */
   public static ClassLoaderRepository getClassLoaderRepository(MBeanServer server)
   {
      return new DefaultLoaderRepositoryWrapper();
   }

   // Private -------------------------------------------------------
   private static MBeanServer createMBeanServer(String defaultDomain, boolean registerServer)
   {
      String serverClass = System.getProperty(
            ServerConstants.MBEAN_SERVER_CLASS_PROPERTY,
            ServerConstants.DEFAULT_MBEAN_SERVER_CLASS
      );

      try
      {
         ClassLoader cl = Thread.currentThread().getContextClassLoader();
         Class clazz = cl.loadClass(serverClass);
         Constructor constructor = clazz.getConstructor(new Class[] { String.class });
         MBeanServer server = (MBeanServer)constructor.newInstance(new Object[] {defaultDomain});

         if (registerServer)
         {
            String agentID = (String)server.getAttribute(
                  new ObjectName(ServerConstants.MBEAN_SERVER_DELEGATE),
                  "MBeanServerId"
            );
            
            serverMap.put(agentID, server);
         }

         return server;
      }
      catch (MalformedObjectNameException e)
      {
         throw new Error(e.toString());
      }
      catch (JMException e)
      {
         throw new Error("Cannot retrieve AgentID: " + e.toString());
      }
      catch (ClassNotFoundException e)
      {
         throw new IllegalArgumentException("The MBean server implementation class " + serverClass + " was not found: " + e.toString());
      }
      catch (NoSuchMethodException e) 
      {
         throw new IllegalArgumentException("The MBean server implementation class " + serverClass + " must contain a default domain string constructor: " + serverClass + "(java.langString defaultDomain)");
      }
      catch (InstantiationException e) 
      {
         throw new IllegalArgumentException("Cannot instantiate class " + serverClass + ": " + e.toString());
      }
      catch (IllegalAccessException e)
      {
         throw new IllegalArgumentException("Unable to create the MBean server instance. Illegal access to class " + serverClass + " constructor: " + e.toString());
      }
      catch (InvocationTargetException e)
      {
         throw new RuntimeException("Unable to create the MBean server instance. Class " + serverClass + " has raised an exception in constructor: " + e.getTargetException().toString());
      }
   }

   private static void checkPermission(MBeanServerPermission permission)
   {
      SecurityManager security = System.getSecurityManager();
      if (security == null)
         return;
      security.checkPermission(permission);
   }

   private static class DefaultLoaderRepositoryWrapper
      implements ClassLoaderRepository
   {
      public Class loadClass(String className)
         throws ClassNotFoundException
      {
         return DefaultLoaderRepository.loadClass(className);
      }

      public Class loadClassWithout(ClassLoader loader, String className)
         throws ClassNotFoundException
      {
         return DefaultLoaderRepository.loadClassWithout(loader, className);
      }
   }
}

