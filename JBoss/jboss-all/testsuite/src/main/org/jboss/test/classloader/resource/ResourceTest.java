/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.resource;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;
import java.util.Enumeration;

import org.jboss.system.ServiceMBeanSupport;

/** A simple service to test resource loading.

 @author Adrian.Brock@HappeningTimes.com
 @author Scott.Stark@jboss.org
 @version $Revision: 1.2.2.5 $
 */
public class ResourceTest
   extends ServiceMBeanSupport
   implements ResourceTestMBean, Runnable
{
   private Exception threadEx;
   private boolean running;
   private String dtdName;

   public String getDtdName()
   {
      return dtdName;
   }
   public void setDtdName(String dtdName)
   {
      this.dtdName = dtdName;
   }

   protected void startService()
      throws Exception
   {
      // Run a thread in the background looking for rsrc using a different loader
      Thread t = new Thread(this, "RsrcLoader");
      synchronized( ResourceTest.class )
      {
         t.start();
         ResourceTest.class.wait();
      }

      loadLocalResource();
      loadGlobalResource();
      findResources();
      running = false;
      t.join();
      if( threadEx != null )
         throw threadEx;
   }

   protected void stopService()
      throws Exception
   {
      running = false;
   }

   /**
    * Checks we can find a local resource in our deployment unit
    */
   public void loadLocalResource()
      throws Exception
   {
      log.info("Looking for resource: META-INF/jboss-service.xml");
      ClassLoader cl = getClass().getClassLoader();
      URL serviceXML = cl.getResource("META-INF/jboss-service.xml");
      if (serviceXML == null)
         throw new Exception("Cannot find META-INF/jboss-service.xml");
      log.info("Found META-INF/jboss-service.xml: "+serviceXML);
      InputStream is = serviceXML.openStream();
      BufferedReader reader = new BufferedReader(new InputStreamReader(is));
      String line = reader.readLine();
      boolean foundService = false;
      while (line != null && foundService == false )
      {
         if (line.indexOf("org.jboss.test.classloader.resource.ResourceTest") != -1)
            foundService = true;
         line = reader.readLine();
      }
      is.close();
      if( foundService == false )
         throw new Exception("Wrong META-INF/jboss-service.xml");

      // Look for the dtds/sample.dtd
      log.info("Looking for resource: "+dtdName);
      URL dtd = cl.getResource(dtdName);
      if( dtd == null )
         throw new Exception("Failed to find "+dtdName);
      log.info("Found "+dtdName+": "+dtd);
   }

   /**
    * Checks we can find a global resource located in the conf dir
    */
   public void loadGlobalResource()
      throws Exception
   {
      ClassLoader loader = getClass().getClassLoader();
      log.info("loadGlobalResource, loader="+loader);
      URL resURL = loader.getResource("standardjboss.xml");
      if (resURL == null)
         throw new Exception("Cannot find standardjboss.xml");
      resURL = loader.getResource("log4j.xml");
      if (resURL == null)
         throw new Exception("Cannot find log4j.xml");
      resURL = loader.getResource("jndi.properties");
      if (resURL == null)
         throw new Exception("Cannot find jndi.properties");
   }

   /** Check that the URLClassLoader.getResources locates the resource
    * across the repository class loader.
    */
   public void findResources()
      throws Exception
   {
      ClassLoader loader = getClass().getClassLoader();
      log.info("findResources, loader="+loader);
      Enumeration resURLs = loader.getResources("META-INF/MANIFEST.MF");
      if ( resURLs == null || resURLs.hasMoreElements() == false )
         throw new Exception("Cannot find META-INF/MANIFEST.MF");
      int count = 0;
      log.debug("Begin META-INF/MANIFESTs");
      while( resURLs.hasMoreElements() )
      {
         URL url = (URL) resURLs.nextElement();
         count ++;
         log.debug(url);
      }
      log.debug("End META-INF/MANIFESTs, count="+count);
      if ( count <= 0 )
         throw new Exception("Did not find multiple META-INF/MANIFEST.MFs");
   }

   /** Load resources in the background to test MT access to the repository
    * during resource lookup
    */
   public void run()
   {
      ClassLoader loader = getClass().getClassLoader();
      do
      {
         synchronized( ResourceTest.class )
         {
            ResourceTest.class.notify();
            log.info("Notified start thread");
         }
         // Load some resouces located from the JavaMail mail.jar
         try
         {
            javax.mail.Session.getInstance(System.getProperties());

            Class sessionClass = loader.loadClass("javax.mail.Session");
            log.info("Loading JavaMail resources using: "+sessionClass.getClassLoader());
            URL resURL = sessionClass.getResource("/META-INF/javamail.default.address.map");
            if( resURL == null )
               throw new Exception("Failed to find javamail.default.address.map");
            resURL = sessionClass.getResource("/META-INF/javamail.default.providers");
            if( resURL == null )
               throw new Exception("Failed to find javamail.default.providers");
            resURL = sessionClass.getResource("/META-INF/javamail.charset.map");
            if( resURL == null )
               throw new Exception("Failed to find javamail.charset.map");
            resURL = sessionClass.getResource("/META-INF/mailcap");
            if( resURL == null )
               throw new Exception("Failed to find mailcap");
            log.info("Found all JavaMail resources");
            // Look for a resource that does not exist
            resURL = sessionClass.getResource("nowhere-to-be-found.xml");
         }
         catch(Exception e)
         {
            threadEx = e;
            log.error("Failed to load resource", e);
            break;
         }
      } while( running );
   }
}
