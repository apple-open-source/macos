/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */
package org.jboss.mx.loading;

import java.util.LinkedList;

/** The UnifiedLoaderRepository4 (ULR) management interface
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface UnifiedLoaderRepository4MBean extends UnifiedLoaderRepositoryMBean
{
   /** Called by LoadMgr to the class loader for the given className
    *@return UnifiedClassLoader3, may be null
    */
   public UnifiedClassLoader4 getClassLoader(String className);

   /** Called by LoadMgr to obtain all class loaders for the given className
    *@return LinkedList<UnifiedClassLoader3>, may be null
    */
   public LinkedList getClassLoaders(String className);

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
