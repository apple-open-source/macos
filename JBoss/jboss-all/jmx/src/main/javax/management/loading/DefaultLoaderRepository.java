/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.loading;

import java.util.Vector;
import java.io.Serializable;
import java.lang.reflect.Method;

import org.jboss.mx.loading.LoaderRepository;
import org.jboss.mx.loading.BasicLoaderRepository;
import org.jboss.mx.server.ServerConstants;
import org.jboss.mx.server.MBeanServerImpl;

/**
 * This class provides static methods for loading classes from the MBean agent's
 * loader repository. <p>
 *
 * The implementation delegates all {@link #loadClass loadClass()} calls to
 * the loader repository initialized for this JVM. The 
 *
 * @see org.jboss.mx.loading.LoaderRepository
 * @see org.jboss.mx.loading.BasicLoaderRepository
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3 $  
 */
public class DefaultLoaderRepository
     implements Serializable
{
   
   // Attributes ----------------------------------------------------
   
   /**
    * List of classloaders added to the repository. This reference is 
    * initialized to the corresponding <tt>loaders</tt> reference in the
    * {@link org.jboss.mx.loading.LoaderRepository#loaders LoaderRepository}
    * class.
    */
   protected static Vector loaders = null;
   
   /**
    * Reference to the actual classloader repository instance we
    * delegate to.
    */
   private static LoaderRepository repository = null;

   // Constructors --------------------------------------------------
   
   /**
    * Default constructor.
    */
   public DefaultLoaderRepository() {}

   // Static --------------------------------------------------------
   
   //
   // Initialize the DLR with the instance defined by the
   // LOADER_REPOSITORY_CLASS_PROPERTY. The default implementation for the
   // repository instance is defined by the DEFAULT_LOADER_REPOSITORY_CLASS
   // property. The static loaders reference is initialized to reference
   // the loader vector in the instantiated repository. CL repository
   // implementations should populate this vector with the classloaders 
   // added to them if they wish to maintain JMX spec compatibility.
   //
   
   static 
   {
      repository = LoaderRepository.getDefaultLoaderRepository();
      loaders = repository.getLoaders();
   }
   
   public static Class loadClass(String className) throws ClassNotFoundException
   {
      return repository.loadClass(className);
   }

   public static Class loadClassWithout(ClassLoader loader, String className) throws ClassNotFoundException
   {
      return repository.loadClassWithout(loader, className);
   }

}


