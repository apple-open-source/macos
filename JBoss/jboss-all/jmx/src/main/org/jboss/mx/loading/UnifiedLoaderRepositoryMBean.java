
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.mx.loading;

import java.net.URL;


/**
 * UnifiedLoaderRepositoryMBean.java
 *
 *
 * Created: Sun Apr 14 13:04:04 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 */

public interface UnifiedLoaderRepositoryMBean 
{
   public UnifiedClassLoader newClassLoader(final URL url, boolean addToRepository)
      throws Exception;
   public UnifiedClassLoader newClassLoader(final URL url, final URL origURL, boolean addToRepository)
      throws Exception;

   public void removeClassLoader(ClassLoader cl);

   public LoaderRepository registerClassLoader(UnifiedClassLoader ucl);

   public LoaderRepository getInstance();

   public URL[] getURLs();

}// UnifiedLoaderRepositoryMBean
