package org.jboss.mx.loading;

import java.lang.reflect.Constructor;
import java.util.HashMap;
import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.management.ObjectName;

import org.jboss.logging.Logger;
import org.jboss.mx.server.ServerConstants;

import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.Node;

/** A factory for LoaderRepository instances. This is used to obtain repository
 * instances for scoped class loading.
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.3 $
 */
public class LoaderRepositoryFactory
{
   /** The JMX name of the parent LoaderRepository */
   public static ObjectName DEFAULT_LOADER_REPOSITORY;
   private static Logger log = Logger.getLogger(LoaderRepositoryFactory.class);
   /** A HashMap<ObjectName, Integer> for the active references to LoaderRepositorys */
   private static HashMap referenceCountMap = new HashMap();

   static
   {
      try
      {
         // Initialize the default LoaderRepository name
         DEFAULT_LOADER_REPOSITORY = new ObjectName(ServerConstants.DEFAULT_LOADER_NAME);
      }
      catch(Exception e)
      {
         log.error("Failed to init DEFAULT_LOADER_REPOSITORY name", e);
      }
   }

   /** A class that represents the configuration of the a LoaderRepository.
    * This defines the JMX ObjectName, the LoaderRepository implementation
    * class, the repository config parser and a string representation of
    * the repository config.
    */
   static public class LoaderRepositoryConfig
   {
      /** The ObjectName of the loader repository for this deployment */
      public ObjectName repositoryName = DEFAULT_LOADER_REPOSITORY;
      public String repositoryClassName;
      public String configParserClassName;
      public String repositoryConfig;

      public String toString()
      {
         StringBuffer tmp = new StringBuffer("LoaderRepositoryConfig(");
         tmp.append("repositoryName: ");
         tmp.append(repositoryName);
         tmp.append(", repositoryClassName: ");
         tmp.append(repositoryClassName);
         tmp.append(", configParserClassName: ");
         tmp.append(configParserClassName);
         tmp.append(", repositoryConfig: ");
         tmp.append(repositoryConfig);
         tmp.append(")");
         return tmp.toString();
      }
   }

   /** The interface representing a LoaderRepository configuration parser. A
    * LoaderRepositoryConfigParser knows how to take a string representation of
    * a config and map that onto a LoaderRepository implementation.
    */
   static public interface LoaderRepositoryConfigParser
   {
      public void configure(LoaderRepository repository, String config)
         throws Exception;
   }

   /** Given a loader-repository element fragment like:
      <loader-repository loaderRepositoryClass='...'>
         jboss.test.cts:loader=cts-cmp2v2.ear
         <loader-repository-config configParserClass='...'>
            ...
         </loader-repository-config>
      </loader-repository>
    create a LoaderRepositoryConfig representation.
    @param config the xml loader-repository element
    @return a LoaderRepositoryConfig representation of the config
    */
   public static LoaderRepositoryConfig parseRepositoryConfig(Element config)
      throws MalformedObjectNameException
   {
      LoaderRepositoryConfig repositoryConfig = new LoaderRepositoryConfig();
      repositoryConfig.repositoryClassName = config.getAttribute("loaderRepositoryClass");
      if( repositoryConfig.repositoryClassName.length() == 0 )
         repositoryConfig.repositoryClassName = ServerConstants.DEFAULT_SCOPED_REPOSITORY_CLASS;

      // Get the object name of the repository
      NodeList children = config.getChildNodes();
      int count = children.getLength();
      if( count > 0 )
      {
         for(int n = 0; n < count; n ++)
         {
            Node node = children.item(n);
            int type = node.getNodeType();
            // Get the ObjectName string
            if( type == Node.TEXT_NODE || type == Node.CDATA_SECTION_NODE )
            {
               String objectName = node.getNodeValue().trim();
               repositoryConfig.repositoryName = new ObjectName(objectName);
               break;
            }
         }

         // Next load any repository config
         children = config.getElementsByTagName("loader-repository-config");
         count = children.getLength();
         if( count > 0 )
         {
            Element loaderRepositoryConfig = (Element) children.item(0);
            children = loaderRepositoryConfig.getChildNodes();
            count = children.getLength();
            repositoryConfig.configParserClassName = loaderRepositoryConfig.getAttribute("configParserClass");
            if( repositoryConfig.configParserClassName.length() == 0 )
               repositoryConfig.configParserClassName = ServerConstants.DEFAULT_SCOPED_REPOSITORY_PARSER_CLASS;
            StringBuffer configData = new StringBuffer();
            for(int n = 0; n < count; n ++)
            {
               Node node = children.item(n);
               int type = node.getNodeType();
               if( type == Node.TEXT_NODE || type == Node.CDATA_SECTION_NODE )
               {
                  configData.append(node.getNodeValue());
               }
            }
            repositoryConfig.repositoryConfig = configData.toString().trim();
         }
      }
      return repositoryConfig;
   }

   /** Create a LoaderRepository instance of type repositoryClassName and
    * register it under repositoryName if there is not already an instance
    * registered.
    *
    * @param server the MBeanServer to register with
    * @param repositoryClassName the class which implements LoaderRepository
    * @param repositoryName the JMX name to register under
    * @throws JMException thrown on any failure to create or register the repository
    */
   public static synchronized void createLoaderRepository(MBeanServer server,
      String repositoryClassName,
      ObjectName repositoryName) throws JMException
   {
      LoaderRepositoryConfig config = new LoaderRepositoryConfig();
      config.repositoryClassName = repositoryClassName;
      config.repositoryName = repositoryName;
      createLoaderRepository(server, config);
   }
   /** Create and configure a LoaderRepository instance using the given config
    * if there is not already an instance registered.
    *
    * @param server the MBeanServer to register with
    * @param config the configuration information which will be used to create
    * register and configure the LoaderRepository instance.
    * @throws JMException thrown on any failure to create or register the repository
    */
   public static synchronized void createLoaderRepository(MBeanServer server,
      LoaderRepositoryConfig config) throws JMException
   {
      if( config == null )
         config = new LoaderRepositoryConfig();
      String repositoryClassName = config.repositoryClassName;
      ObjectName repositoryName = config.repositoryName;

      try
      {
         ObjectInstance oi = server.getObjectInstance(repositoryName);
         if ( (repositoryClassName != null) &&
               !oi.getClassName().equals(repositoryClassName) )
         {
            throw new JMException("Inconsistent LoaderRepository class specification in repository: " + repositoryName);
         } // end of if ()
      }
      catch (InstanceNotFoundException e)
      {
         //we are the first, make the repository.
         if( repositoryClassName == null )
            repositoryClassName = ServerConstants.DEFAULT_SCOPED_REPOSITORY_CLASS;

         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         LoaderRepository repository = null;
         try
         {
            // Create the repository loader
            Class repositoryClass = loader.loadClass(repositoryClassName);
            Class[] ctorSig = {MBeanServer.class, ObjectName.class};
            Constructor ctor = repositoryClass.getConstructor(ctorSig);
            Object[] args = {server, DEFAULT_LOADER_REPOSITORY};
            repository = (LoaderRepository) ctor.newInstance(args);
            server.registerMBean(repository, repositoryName);
         }
         catch(Exception e2)
         {
            log.debug("Failed to create loader repository: ", e2);
            throw new JMException("Failed to create loader repository:" + e2);
         }

         try
         {
            // Configure the repository
            if( config.configParserClassName != null && config.repositoryConfig != null )
            {
               Class parserClass = loader.loadClass(config.configParserClassName);
               LoaderRepositoryConfigParser parser = (LoaderRepositoryConfigParser)
                  parserClass.newInstance();
               parser.configure(repository, config.repositoryConfig);
            }
         }
         catch(Exception e2)
         {
            log.debug("Failed to configure loader repository: ", e2);
            throw new JMException("Failed to configure loader repository: "+e2);
         }
      } // end of try-catch

      Integer activeCount = (Integer) referenceCountMap.get(repositoryName);
      if( activeCount == null )
         activeCount = new Integer(1);
      else
         activeCount = new Integer(activeCount.intValue() + 1);
      referenceCountMap.put(repositoryName, activeCount);
   }

   public static synchronized void destroyLoaderRepository(MBeanServer server,
      ObjectName repositoryName)
   {
      if( repositoryName.equals(DEFAULT_LOADER_REPOSITORY) == false )
      {
         try
         {
            Integer activeCount = (Integer) referenceCountMap.get(repositoryName);
            if( activeCount != null )
            {
               if( activeCount.intValue() == 1 )
               {
                  server.unregisterMBean(repositoryName);
                  referenceCountMap.remove(repositoryName);
                  log.debug("Unregistered repository: "+repositoryName);
               }
               else
               {
                  activeCount = new Integer(activeCount.intValue() - 1);
                  referenceCountMap.put(repositoryName, activeCount);
               }
            }
         }
         catch(Exception e)
         {
            log.warn("Failed to unregister ear loader repository", e);
         }
      }
   }

   private LoaderRepositoryFactory()
   {
   }
}// LoaderRepositoryFactory
