/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.mx.loading;

import java.util.HashSet;

/** The UnifiedLoaderRepository3 (ULR) management interface
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.4 $
 */
public interface UnifiedLoaderRepository3MBean extends UnifiedLoaderRepositoryMBean
{
   /** Called by LoadMgr to obtain all class loaders for the given className
    *@return LinkedList<UnifiedClassLoader3>, may be null
    */
   public HashSet getPackageClassLoaders(String className);

   /** A utility method that iterates over all repository class loader and
    * display the class information for every UCL that contains the given
    * className
    */
   public String displayClassInfo(String className);

   /** Get the number of classes loaded into the ULR cache.
    * @return the classes cache size.
    */
   public int getCacheSize();
   /** Get the number of UnifiedClassLoader3s (UCLs) in the ULR
    * @return the number of UCLs in the ULR
    */
   public int getClassLoadersSize();
   /** Flush the ULR classes cache
    */
   public void flush();
}
