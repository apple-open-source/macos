/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system.server;

import java.util.Enumeration;
import java.util.Iterator;

import java.lang.reflect.Method;

import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MBeanRegistration;

import org.jboss.logging.Logger;
import org.jboss.mx.util.ObjectNameFactory;
import org.jboss.mx.server.ServerConstants;

import org.jboss.util.platform.Java;

/**
 * An MBean that provides a rich view of system information for the JBoss
 * server in which it is deployed.
 *
 * @jmx:mbean name="jboss.system:type=ServerInfo"
 *
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:Scott.Stark@jboss.org">Scott Stark</a>
 * @author <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.9.2.1 $
 */
public class ServerInfo
   implements ServerInfoMBean, MBeanRegistration
{
   public static final ObjectName DEFAULT_LOADER_REPOSITORY = ObjectNameFactory.create(ServerConstants.DEFAULT_LOADER_NAME);

   /** Class logger. */
   private static final Logger log = Logger.getLogger(ServerInfo.class);
   
   /** The cached host name for the server. */
   private String hostName;
   
   /** The cached host address for the server. */
   private String hostAddress;

   private MBeanServer server;
   
   
   ///////////////////////////////////////////////////////////////////////////
   //                               JMX Hooks                               //
   ///////////////////////////////////////////////////////////////////////////
   
   public ObjectName preRegister(MBeanServer server, ObjectName name)
      throws Exception
   {
      this.server = server;
      // Dump out basic JVM & OS info as INFO priority msgs
      log.info("Java version: " +
      System.getProperty("java.version") + "," +
      System.getProperty("java.vendor"));
      
      log.info("Java VM: " +
      System.getProperty("java.vm.name") + " " +
      System.getProperty("java.vm.version") + "," +
      System.getProperty("java.vm.vendor"));
      
      log.info("OS-System: " +
      System.getProperty("os.name") + " " +
      System.getProperty("os.version") + "," +
      System.getProperty("os.arch"));
      
      // Dump out the entire system properties if debug is enabled
      if (log.isDebugEnabled())
      {
         log.debug("Full System Properties Dump");
         Enumeration names = System.getProperties().propertyNames();
         while (names.hasMoreElements())
         {
            String pname = (String)names.nextElement();
            log.debug("    " + pname + ": " + System.getProperty(pname));
         }
      }
      
      return name == null ? OBJECT_NAME : name;
   }
   
   public void postRegister(Boolean registrationDone)
   {
      // empty
   }
   
   public void preDeregister() throws Exception
   {
      // empty
   }
   
   public void postDeregister()
   {
      // empty
   }
   
   
   ///////////////////////////////////////////////////////////////////////////
   //                            Server Information                         //
   ///////////////////////////////////////////////////////////////////////////

   /**
    * @jmx:managed-attribute
    */
   public String getJavaVersion()
   {
      return System.getProperty("java.version");
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJavaVendor()
   {
      return System.getProperty("java.vendor");
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJavaVMName()
   {
      return System.getProperty("java.vm.name");
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJavaVMVersion()
   {
      return System.getProperty("java.vm.version");
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJavaVMVendor()
   {
      return System.getProperty("java.vm.vendor");
   }

   /**
    * @jmx:managed-attribute
    */
   public String getOSName()
   {
      return System.getProperty("os.name");
   }

   /**
    * @jmx:managed-attribute
    */
   public String getOSVersion()
   {
      return System.getProperty("os.version");
   }

   /**
    * @jmx:managed-attribute
    */
   public String getOSArch()
   {
      return System.getProperty("os.arch");
   }
   
   /**
    * @jmx:managed-attribute
    */
   public Long getTotalMemory()
   {
      return new Long(Runtime.getRuntime().totalMemory());
   }
   
   /**
    * @jmx:managed-attribute
    */
   public Long getFreeMemory()
   {
      return new Long(Runtime.getRuntime().freeMemory());
   }
   
   /**
    * Returns <tt>Runtime.getRuntime().maxMemory()<tt> on 
    * JDK 1.4 vms or -1 on previous versions.
    * 
    * @jmx:managed-attribute
    */
   public Long getMaxMemory()
   {
      if (Java.isCompatible(Java.VERSION_1_4)) {
         // Uncomment when JDK 1.4 is the base JVM
         // return new Long(Runtime.getRuntime().maxMemory());

         // until then use reflection to do the job
         try {
            Runtime rt = Runtime.getRuntime();
            Method m = rt.getClass().getMethod("maxMemory", new Class[0]);
            return (Long)m.invoke(rt, new Object[0]);
         }
         catch (Exception e) {
            log.error("Operation failed", e);
         }
      }

      return new Long(-1);
   }

   /**
    * Returns <tt>Runtime.getRuntime().availableProcessors()</tt> on 
    * JDK 1.4 vms or -1 on previous versions.
    * 
    * @jmx:managed-attribute
    */
   public Integer getAvailableProcessors()
   {
      if (Java.isCompatible(Java.VERSION_1_4)) {
         // Uncomment when JDK 1.4 is the base JVM
         // return new Integer(Runtime.getRuntime().availableProcessors());

         // until then use reflection to do the job
         try {
            Runtime rt = Runtime.getRuntime();
            Method m = rt.getClass().getMethod("availableProcessors", new Class[0]);
            return (Integer)m.invoke(rt, new Object[0]);
         }
         catch (Exception e) {
            log.error("Operation failed", e);
         }
      }

      return new Integer(-1);
   }

   /**
    * Returns InetAddress.getLocalHost().getHostName();
    *
    * @jmx:managed-attribute
    */
   public String getHostName()
   {
      if (hostName == null)
      {
         try
         {
            hostName = java.net.InetAddress.getLocalHost().getHostName();
         }
         catch (java.net.UnknownHostException e)
         {
            log.error("Error looking up local hostname", e);
            hostName = "<unknown>";
         }
      }
      
      return hostName;
   }
   
   /**
    * Returns InetAddress.getLocalHost().getHostAddress();
    *
    * @jmx:managed-attribute
    */
   public String getHostAddress()
   {
      if (hostAddress == null)
      {
         try
         {
            hostAddress = java.net.InetAddress.getLocalHost().getHostAddress();
         }
         catch (java.net.UnknownHostException e)
         {
            log.error("Error looking up local address", e);
            hostAddress = "<unknown>";
         }
      }
      
      return hostAddress;
   }

   private ThreadGroup getRootThreadGroup()
   {
      ThreadGroup group = Thread.currentThread().getThreadGroup();
      while (group.getParent() != null)
      {
         group = group.getParent();
      }

      return group;
   }

   /**
    * @jmx:managed-operation
    */
   public Integer getActiveThreadCount()
   {
      return new Integer(getRootThreadGroup().activeCount());
   }

   /**
    * @jmx:managed-operation
    */
   public Integer getActiveThreadGroupCount()
   {
      return new Integer(getRootThreadGroup().activeGroupCount());
   }
   
   /**
    * Return a listing of the active threads and thread groups.
    *
    * @jmx:managed-operation
    */
   public String listThreadDump()
   {
      ThreadGroup root = getRootThreadGroup();
      
      // I'm not sure why what gets reported is off by +1,
      // but I'm adjusting so that it is consistent with the display
      int activeThreads = root.activeCount()-1;
      // I'm not sure why what gets reported is off by -1
      // but I'm adjusting so that it is consistent with the display
      int activeGroups = root.activeGroupCount()+1;
      
      String rc=
      "<b>Total Threads:</b> "+activeThreads+"<br>"+
      "<b>Total Thread Groups:</b> "+activeGroups+"<br>"+
      getThreadGroupInfo(root) ;
      return rc;
   }


   private String getThreadGroupInfo(ThreadGroup group)
   {
      StringBuffer rc = new StringBuffer();
      
      rc.append("<BR><B>");
      rc.append("Thread Group: " + group.getName());
      rc.append("</B> : ");
      rc.append("max priority:" + group.getMaxPriority() +
                ", demon:" + group.isDaemon());
      
      rc.append("<blockquote>");
      Thread threads[]= new Thread[group.activeCount()];
      group.enumerate(threads, false);
      for (int i= 0; i < threads.length && threads[i] != null; i++)
      {
         rc.append("<B>");
         rc.append("Thread: " + threads[i].getName());
         rc.append("</B> : ");
         rc.append("priority:" + threads[i].getPriority() +
         ", demon:" + threads[i].isDaemon());
         rc.append("<BR>");
      }
      
      ThreadGroup groups[]= new ThreadGroup[group.activeGroupCount()];
      group.enumerate(groups, false);
      for (int i= 0; i < groups.length && groups[i] != null; i++)
      {
         rc.append(getThreadGroupInfo(groups[i]));
      }
      rc.append("</blockquote>");
      
      return rc.toString();
   }

   /**
    * Display the java.lang.Package info for the pkgName
    *
    * @jmx:managed-operation
    */
   public String displayPackageInfo(String pkgName)
   {
      Package pkg = Package.getPackage(pkgName);
      if( pkg == null )
         return "<h2>Package:"+pkgName+" Not Found!</h2>";

      StringBuffer info = new StringBuffer("<h2>Package: "+pkgName+"</h2>");
      displayPackageInfo(pkg, info);
      return info.toString();
   }

   /** 
    * Display the ClassLoader, ProtectionDomain and Package information for
    * the specified class.
    *
    * @return a simple html report of this information
    *
    * @jmx:managed-operation
    */
   public String displayInfoForClass(String className) throws Exception
   {
      Class clazz = (Class)server.invoke(DEFAULT_LOADER_REPOSITORY, 
                                  "findClass", 
                                  new Object[] {className}, 
                                  new String[] {String.class.getName()});
      if( clazz == null )
         return "<h2>Class:"+className+" Not Found!</h2>";
      Package pkg = clazz.getPackage();
      if( pkg == null )
         return "<h2>Class:"+className+" has no Package info</h2>";

      StringBuffer info = new StringBuffer("<h1>Class: "+pkg.getName()+"</h1>");
      ClassLoader cl = clazz.getClassLoader();
      info.append("<h2>ClassLoader: "+cl+"</h2>\n");
      info.append("<h3>ProtectionDomain</h3>\n");
      info.append("<pre>\n"+clazz.getProtectionDomain()+"</pre>\n");
      info.append("<h2>Package: "+pkg.getName()+"</h2>");
      displayPackageInfo(pkg, info);
      return info.toString();
   }

   /** 
    * This does not work as expected because the thread context class loader
    * is not used to determine which class loader the package list is obtained
    * from.
    */
   public String displayAllPackageInfo()
   {
      return "Broken right now";
      /*
      ClassLoader entryCL = Thread.currentThread().getContextClassLoader();
      ServiceLibraries libraries = ServiceLibraries.getLibraries();
      ClassLoader[] classLoaders = libraries.getClassLoaders();
      StringBuffer info = new StringBuffer();
      for(int c = 0; c < classLoaders.length; c ++)
      {
         ClassLoader cl = classLoaders[c];
         Thread.currentThread().setContextClassLoader(cl);
         try
         {
            info.append("<h1>ClassLoader: "+cl+"</h1>\n");
            Package[] pkgs = Package.getPackages();
            for(int p = 0; p < pkgs.length; p ++)
            {
               Package pkg = pkgs[p];
               info.append("<h2>Package: "+pkg.getName()+"</h2>\n");
               displayPackageInfo(pkg, info);
            }
         }
         catch(Throwable e)
         {
         }
      }
      Thread.currentThread().setContextClassLoader(entryCL);
      return info.toString();
      */
   }

   private void displayPackageInfo(Package pkg, StringBuffer info)
   {
      info.append("<pre>\n");
      info.append("SpecificationTitle: "+pkg.getSpecificationTitle());
      info.append("\nSpecificationVersion: "+pkg.getSpecificationVersion());
      info.append("\nSpecificationVendor: "+pkg.getSpecificationVendor());
      info.append("\nImplementationTitle: "+pkg.getImplementationTitle());
      info.append("\nImplementationVersion: "+pkg.getImplementationVersion());
      info.append("\nImplementationVendor: "+pkg.getImplementationVendor());
      info.append("\nisSealed: "+pkg.isSealed());
      info.append("</pre>\n");
   }
}
