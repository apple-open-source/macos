/*
 * LGPL
 */
package javax.management.loading;

import javax.management.ServiceNotFoundException;

public interface MLetMBean {

   public java.util.Set getMBeansFromURL(java.lang.String url) throws ServiceNotFoundException;

   public java.util.Set getMBeansFromURL(java.net.URL url) throws ServiceNotFoundException;

   public void addURL(java.net.URL url);

   public void addURL(java.lang.String url) throws ServiceNotFoundException;

   public java.net.URL[] getURLs();

   public java.net.URL getResource(java.lang.String name);

   public java.io.InputStream getResourceAsStream(java.lang.String name);

   public java.util.Enumeration getResources(java.lang.String name) throws java.io.IOException;

   public java.lang.String getLibraryDirectory();

   public void setLibraryDirectory(java.lang.String libdir);


}

