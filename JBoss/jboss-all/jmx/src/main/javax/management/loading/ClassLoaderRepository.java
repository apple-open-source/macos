/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.loading;

/**
 * A classloader repository.<p>
 *
 * A loader repository per MBeanServer.
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 *
 * @version $Revision: 1.2.2.1 $
 */
public interface ClassLoaderRepository
{
   /**
    * Loads a class from the repository. This method attempts to load the class
    * using all the classloader registered to the repository.
    *
    * @param className the class to load
    * @return the found class
    * @exception ClassNotFoundException when there is no such class
    */
   Class loadClass(String className) throws ClassNotFoundException;

   /**
    * Loads a class from the repository, excluding the given
    * classloader. 
    *
    * @param loader the classloader to exclude
    * @param className the class to load
    * @return the found class
    * @exception ClassNotFoundException when there is no such class
    */
   Class loadClassWithout(ClassLoader loader, String className)
      throws ClassNotFoundException;
}
