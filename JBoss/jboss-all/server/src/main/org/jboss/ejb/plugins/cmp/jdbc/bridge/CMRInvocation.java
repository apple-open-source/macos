package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import org.jboss.invocation.LocalEJBInvocation;
import org.jboss.ejb.plugins.lock.Entrancy;

import javax.transaction.Transaction;
import java.lang.reflect.Method;
import java.security.Principal;

/**
 * Optimized invocation object for local CMR invocations
 *
 * @author  <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.1.2.2 $
 */
public final class CMRInvocation extends LocalEJBInvocation
{
   private Entrancy entrancy;
   private CMRMessage cmrMessage;

   public CMRInvocation()
   {
   }

   public Entrancy getEntrancy()
   {
      return entrancy;
   }

   public void setEntrancy(Entrancy entrancy)
   {
      this.entrancy = entrancy;
   }

   public CMRMessage getCmrMessage()
   {
      return cmrMessage;
   }

   public void setCmrMessage(CMRMessage cmrMessage)
   {
      this.cmrMessage = cmrMessage;
   }
}
