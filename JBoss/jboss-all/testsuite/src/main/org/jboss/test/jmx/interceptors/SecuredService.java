package org.jboss.test.jmx.interceptors;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.logging.Logger;

/** A xmbean implementation class that employs a custom interceptor to
 * prevent secretXXX operations from being invoked.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class SecuredService extends ServiceMBeanSupport
{
   private static Logger log = Logger.getLogger(SecuredService.class);

   public String echo(String arg)
   {
      log.info("echo, arg="+arg);
      return arg;
   }
   public String secretEcho(String arg)
   {
      log.info("secretEcho, arg="+arg);
      return arg;
   }
}
