package org.jboss.test.security.service;

import java.security.Principal;
import org.jboss.system.ServiceMBeanSupport;

/** An xmbean service that requires an SRP session to invokes its testSession operation
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class SRPCacheTest extends ServiceMBeanSupport
{
   private int callCount;

   public synchronized int testSession(Principal principal, byte[] key)
   {
      callCount ++;
      log.info("testSession called, callCount="+callCount+", principal="+principal);
      return callCount;
   }
}
