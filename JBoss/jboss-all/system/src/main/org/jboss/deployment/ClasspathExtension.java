/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.deployment;

import java.net.URL;
import javax.management.ObjectName;
import org.jboss.mx.loading.LoaderRepositoryFactory;
import org.jboss.mx.loading.UnifiedClassLoader;
import org.jboss.system.ServiceMBeanSupport;


/** A service that allows one to add an arbitrary URL to a named LoaderRepository. 
 *
 * Created: Sun Jun 30 13:17:22 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.2.2.2 $
 *
 * @jmx:mbean name="jboss:type=Service,service=ClasspathExtension"
 *            extends="org.jboss.system.ServiceMBean"
 */
public class ClasspathExtension
   extends ServiceMBeanSupport
   implements ClasspathExtensionMBean
{
   private String metadataURL;
   private ObjectName loaderRepository;
   private UnifiedClassLoader ucl;

   public ClasspathExtension() 
   {
      
   }

   /**
    * mbean get-set pair for field metadataURL
    * Get the value of metadataURL
    * @return value of metadataURL
    *
    * @jmx:managed-attribute
    */
   public String getMetadataURL()
   {
      return metadataURL;
   }

   /**
    * Set the value of metadataURL
    * @param metadataURL  Value to assign to metadataURL
    *
    * @jmx:managed-attribute
    */
   public void setMetadataURL(String metadataURL)
   {
      this.metadataURL = metadataURL;
   }

   /**
    * mbean get-set pair for field loaderRepository
    * Get the value of loaderRepository
    * @return value of loaderRepository
    *
    * @jmx:managed-attribute
    */
   public ObjectName getLoaderRepository()
   {
      return loaderRepository;
   }
   
   
   /**
    * Set the value of loaderRepository
    * @param loaderRepository  Value to assign to loaderRepository
    *
    * @jmx:managed-attribute
    */
   public void setLoaderRepository(ObjectName loaderRepository)
   {
      this.loaderRepository = loaderRepository;
   }

   protected void createService() throws Exception
   {
      if (metadataURL != null) 
      {
         URL url = new URL(metadataURL);
         if( loaderRepository == null )
            loaderRepository = LoaderRepositoryFactory.DEFAULT_LOADER_REPOSITORY;
         Object[] args = {url, url, Boolean.TRUE};
         String[] sig = {"java.net.URL", "java.net.URL", "boolean"};
         ucl = (UnifiedClassLoader) server.invoke(loaderRepository,
            "newClassLoader",args, sig);
      } // end of if ()
   }

   protected void destroyService() throws Exception
   {
      if (ucl != null) 
      {
         ucl.unregister();
      } // end of if ()
      
   }

}// ClasspathExtension
