/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web;

import java.net.URL;

import org.jboss.metadata.WebMetaData;
import org.jboss.deployment.DeploymentInfo;

/** A WebApplication represents the information for a war deployment.

 @see AbstractWebContainer

 @author Scott.Stark@jboss.org
 @version $Revision: 1.6.2.1 $
 */
public class WebApplication
{
   /** Class loader of this application */
   ClassLoader classLoader = null;
   /** name of this application */
   String name = "";
   /** URL where this application was deployed from */
   URL url;
   /** The web app metadata from the web.xml and jboss-web.xml descriptors */
   WebMetaData metaData;
   /** Arbitary data object for storing application specific data */
   Object data;
   /** The DeploymentInfo for the war */
   DeploymentInfo warInfo;

   /** Create an empty WebApplication instance
    */
   public WebApplication()
   {
   }

   /** Create a WebApplication instance with with given web-app metadata.
    @param metaData, the web-app metadata containing the web.xml and
    jboss-web.xml descriptor metadata.
    */
   public WebApplication(WebMetaData metaData)
   {
      this.metaData = metaData;
   }

   /** Create a WebApplication instance with with given name,
    url and class loader.
    @param name, name of this application
    @param url, url where this application was deployed from
    @param classLoader, Class loader of this application
    */
   public WebApplication(String name, URL url, ClassLoader classLoader)
   {
      this.name = name;
      this.url = url;
      this.classLoader = classLoader;
   }

   /** Get the class loader of this WebApplication.
    * @return The ClassLoader instance of the web application
    */
   public ClassLoader getClassLoader()
   {
      return classLoader;
   }

   /** Set the class loader of this WebApplication.
    * @param classLoader, The ClassLoader instance for the web application
    */
   public void setClassLoader(ClassLoader classLoader)
   {
      this.classLoader = classLoader;
   }

   /** Get the name of this WebApplication.
    * @return String name of the web application
    */
   public String getName()
   {
      String n = name;
      if (n == null)
         n = url.getFile();
      return n;
   }

   /** Set the name of this WebApplication.
    * @param String name of the web application
    */
   public void setName(String name)
   {
      this.name = name;
   }


   /** Get the URL from which this WebApplication was deployed
    * @return URL where this application was deployed from
    */
   public URL getURL()
   {
      return url;
   }

   /** Set the URL from which this WebApplication was deployed
    * @param url, URL where this application was deployed from
    */
   public void setURL(URL url)
   {
      if (url == null)
         throw new IllegalArgumentException("Null URL");
      this.url = url;
   }

   /** Getter for property metaData.
    * @return Value of property metaData.
    */
   public WebMetaData getMetaData()
   {
      return metaData;
   }

   /** Setter for property metaData.
    * @param metaData New value of property metaData.
    */
   public void setMetaData(WebMetaData metaData)
   {
      this.metaData = metaData;
   }

   public Object getAppData()
   {
      return data;
   }

   public void setAppData(Object data)
   {
      this.data = data;
   }

   public DeploymentInfo getDeploymentInfo()
   {
      return warInfo;
   }

   public void setDeploymentInfo(DeploymentInfo warInfo)
   {
      this.warInfo = warInfo;
   }

   public String toString()
   {
      StringBuffer buffer = new StringBuffer("{WebApplication: ");
      buffer.append(getName());
      buffer.append(", URL: ");
      buffer.append(url);
      buffer.append(", classLoader: ");
      buffer.append(classLoader);
      buffer.append(':');
      buffer.append(classLoader.hashCode());
      buffer.append('}');
      return buffer.toString();
   }
}
