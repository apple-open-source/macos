package org.jboss.invocation;

import javax.transaction.Transaction;
import java.lang.reflect.Method;
import java.security.Principal;
import java.util.Map;

/**
 * Optimized invocation object for Local interface invocations
 *
 * @author  <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.1.2.1 $
 */
public class LocalEJBInvocation extends Invocation
{
   public LocalEJBInvocation()
   {
   }

   public LocalEJBInvocation(Object id, Method m, Object[] args, Transaction tx,
                Principal identity, Object credential)
   {
      super(id, m, args, tx, identity, credential);
   }

   private Transaction tx;
   private Object credential;
   private Principal principal;
   private Object enterpriseContext;
   private Object id;

   public void setTransaction(Transaction tx)
   {
     this.tx = tx;
   }

   public Transaction getTransaction()
   {
      return this.tx;
   }

   public Object getCredential()
   {
      return credential;
   }

   public void setCredential(Object credential)
   {
      this.credential = credential;
   }

   public Principal getPrincipal()
   {
      return principal;
   }

   public void setPrincipal(Principal principal)
   {
      this.principal = principal;
   }

   public Object getEnterpriseContext()
   {
      return enterpriseContext;
   }

   public void setEnterpriseContext(Object enterpriseContext)
   {
      this.enterpriseContext = enterpriseContext;
   }

   public Object getId()
   {
      return id;
   }

   public void setId(Object id)
   {
      this.id = id;
   }
}
