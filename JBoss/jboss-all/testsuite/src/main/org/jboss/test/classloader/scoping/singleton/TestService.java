package org.jboss.test.classloader.scoping.singleton;

import java.security.CodeSource;

import org.jboss.system.ServiceMBeanSupport;

/** A service that validates that its version of the MySingleton corresponds
 * the version passed into the checkVersion operation.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class TestService extends ServiceMBeanSupport
   implements TestServiceMBean
{
   protected void startService()
   {
      MySingleton singleton = MySingleton.getInstance();
      log.debug("Start called, singleton="+singleton);
      log.debug("Singleton version="+singleton.getVersion());
   }

   public boolean checkVersion(String version)
   {
      MySingleton singleton = MySingleton.getInstance();
      CodeSource cs = singleton.getClass().getProtectionDomain().getCodeSource();
      log.debug("MySingleton.CS: "+cs);
      log.debug("Comparing version: "+version+", to: "+singleton.getVersion());
      return version.equals(singleton.getVersion());
   }

}
