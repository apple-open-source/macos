/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.proxy;

import javax.transaction.TransactionManager;
import javax.transaction.Transaction;

import org.jboss.invocation.Invocation;
import org.jboss.proxy.Interceptor;

/**
* The client-side proxy for an EJB Home object.
*      
* @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
* @version $Revision: 1.4.2.2 $
*/
public class TransactionInterceptor
   extends Interceptor
{
   /** Serial Version Identifier. @since 1.4.2.1 */
   private static final long serialVersionUID = 371972342995600888L;

   public static TransactionManager tm;

   /**
   * No-argument constructor for externalization.
   */
   public TransactionInterceptor()
   {
   }

   // Public --------------------------------------------------------
   
   public Object invoke(Invocation invocation) 
   throws Throwable
   {
      if (tm != null)
      {
         Transaction tx = tm.getTransaction();
         if (tx != null) invocation.setTransaction(tx);
      }
      return getNext().invoke(invocation);
   }
   
   
   /** Transaction manager. */
   public static void setTransactionManager(TransactionManager tmx)
   {
      tm = tmx;
   }
}
