/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.util;

import javax.management.InstanceNotFoundException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.ReflectionException;

import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.loading.MBeanElement;

import java.util.Map;
import java.util.List;
import java.util.HashMap;
import java.util.Date;

/**
 * MBean installer utility<p>
 *
 * This installer allows MLet to install or upgrade a mbean based on the version
 * specified in the MLet conf file. If the mbean version is newer than the registered
 * in the server, the installer unregisters the old mbean and then registers the new one.
 * This management needs to store the mbean version into the MBeanRegistry in the server.
 *
 * When we register mbeans, however, we can't pass the metadata to MBeanServer through the
 * standard JMX api because Both of createMBean() and registerMBean() have no extra arguments
 * to attach the metadata. Thus we call MBeanServer#invoke() directly to set/get the internal
 * mbean metadata.
 *
 * Currently version and date are stored in the mbean registry as mbean metadata.
 * The date will be used for preparing presentaionString for this mbean info.
 * For managment purpose, we can add any extra data to the matadata if you need.
 *
 * @author  <a href="mailto:Fusayuki.Minamoto@fujixerox.co.jp">Fusayuki Minamoto</a>.
 * @version $Revision: 1.1 $
 *
 */
public class MBeanInstaller
{
   public static final String VERSIONS = "versions";
   public static final String DATE     = "date";

   private MBeanServer server;
   private ClassLoader loader;
   private ObjectName  loaderName;
   private ObjectName  registryName;

   public MBeanInstaller(MBeanServer server, ClassLoader loader, ObjectName loaderName)
      throws Exception
   {
      this.server = server;
      this.loader = loader;
      this.loaderName   = loaderName;
      this.registryName = new ObjectName(ServerConstants.MBEAN_REGISTRY);
   }

   /**
    * Install a mbean with mbean metadata<p>
    *
    * @param element MBeanElement
    * @return mbean instance
    */
   public ObjectInstance installMBean(MBeanElement element)
      throws MBeanException,
             ReflectionException,
             InstanceNotFoundException,
             MalformedObjectNameException
   {
      ObjectInstance instance = null;

      if (element.getVersions() == null)
         instance = createMBean(element);
      else
         instance = updateMBean(element);

      return instance;
   }

   public ObjectInstance createMBean(MBeanElement element)
      throws MBeanException,
             ReflectionException,
             InstanceNotFoundException,
             MalformedObjectNameException
   {
      ObjectName elementName = getElementName(element);

      // Set up the valueMap passing to the registry.
      // This valueMap contains mbean meta data and update time.
      Map valueMap = createValueMap(element);

      // Unregister previous mbean
      if (server.isRegistered(elementName))
         unregisterMBean(elementName);

      // Create the mbean instance
      Object instance = server.instantiate(
            element.getCode(),
            loaderName,
            element.getConstructorValues(),
            element.getConstructorTypes());

      // Call MBeanRegistry.invoke("registerMBean") instead of server.registerMBean() to pass
      // the valueMap that contains management values including mbean metadata and update time.
      return registerMBean(instance, elementName, valueMap);
   }

   public ObjectInstance updateMBean(MBeanElement element)
      throws MBeanException,
             ReflectionException,
             InstanceNotFoundException,
             MalformedObjectNameException
   {
      ObjectName elementName = getElementName(element);

      // Compare versions to decide whether to skip installation of this mbean
      MLetVersion preVersion  = new MLetVersion(getVersions(elementName));
      MLetVersion newVersion  = new MLetVersion(element.getVersions());

      // FIXME: this comparison works well only if both versions are specified
      //        because jmx spec doesn't fully specify this behavior.
      if (preVersion.isNull() || newVersion.isNull() || preVersion.compareTo(newVersion) < 0)
      {
         // Create mbean with value map
         return createMBean(element);
      }

      return server.getObjectInstance(elementName);
   }

   private ObjectName getElementName(MBeanElement element) throws MalformedObjectNameException
   {
      return (element.getName() != null) ? new ObjectName(element.getName()) : null;
   }

   private Map createValueMap(MBeanElement element)
   {
      HashMap valueMap = new HashMap();

      // We need to set versions here because we can't get the mbean entry
      // outside the server.
      if (element.getVersions() != null)
         valueMap.put(VERSIONS, element.getVersions());

      // The date would be used to make a presentationString for this mbean.
      valueMap.put(DATE, new Date(System.currentTimeMillis()));

      // The valueMap is supposed to have class loader.
      // see MBeanEntry constructor
      valueMap.put(ServerConstants.CLASSLOADER, loader);

      return valueMap;
   }

   private List getVersions(ObjectName name)
         throws MBeanException, ReflectionException, InstanceNotFoundException
   {
      if (! server.isRegistered(name))
         return null;

      return (List) getValue(name, VERSIONS);
   }


   private Object getValue(ObjectName name, String key)
      throws MBeanException, ReflectionException, InstanceNotFoundException
   {
      Object value =
            server.invoke(registryName, "getValue",
                          new Object[]
                          {
                             name,
                             key
                          },
                          new String[]
                          {
                             ObjectName.class.getName(),
                             String.class.getName()
                          }
            );

      return value;
   }

   private ObjectInstance registerMBean(Object object, ObjectName name, Map valueMap)
      throws MBeanException, ReflectionException, InstanceNotFoundException
   {
      return (ObjectInstance)
            server.invoke(registryName, "registerMBean",
                          new Object[]
                          {
                             object,
                             name,
                             valueMap
                          },
                          new String[]
                          {
                             Object.class.getName(),
                             ObjectName.class.getName(),
                             Map.class.getName()
                          }
            );
   }

   private void unregisterMBean(ObjectName name)
      throws MBeanException, ReflectionException, InstanceNotFoundException
   {
      server.invoke(registryName, "unregisterMBean",
                    new Object[]
                    {
                       name,
                    },
                    new String[]
                    {
                       ObjectName.class.getName(),
                    }
      );
   }
}

/**
 * MLetVersion for encapsulating the version representation<p>
 *
 * Because this class is comparable, you can elaborate the
 * version comparison algorithm if you need better one.
 */
class MLetVersion implements Comparable
{
   protected List versions;

   public MLetVersion(List versions)
   {
      this.versions = versions;
   }

   public List getVersions()
   {
      return versions;
   }

   public boolean isNull()
   {
      return versions == null;
   }

   public int compareTo(Object o)
   {
      MLetVersion other = (MLetVersion) o;

      if (isNull() || other.isNull())
         throw new IllegalArgumentException("MLet versions is null");

      // FIXME: this compares only first element of the versions.
      //        do we really need multiple versions?
      String thisVersion = (String) versions.get(0);
      String otherVersion = (String) other.getVersions().get(0);

      return (thisVersion.compareTo(otherVersion));
   }
}
