/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.proxy.ejb;

import java.lang.reflect.Method;

import javax.ejb.EJBHome;
import javax.ejb.EJBMetaData;
import javax.ejb.RemoveException;
import javax.ejb.Handle;
import javax.ejb.EJBHome;
import javax.ejb.EJBObject;
import javax.ejb.HomeHandle;

import org.jboss.proxy.ejb.handle.HomeHandleImpl;
import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationContext;
import org.jboss.invocation.InvocationKey;
import org.jboss.invocation.InvocationType;

/**
 * The client-side proxy for a stateless session Home object,
 * that caches the stateless session interface
 *      
 * @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 * @version $Revision: 1.1.2.1 $
 */
public class StatelessSessionHomeInterceptor
   extends HomeInterceptor
{
   /** Serial Version Identifier */
   private static final long serialVersionUID = 1333656107035759719L;
   
   // Attributes ----------------------------------------------------

   /**
    * The cached interface
    */
   Object cached;
   
   // Constructors --------------------------------------------------
   
   /**
   * No-argument constructor for externalization.
   */
   public StatelessSessionHomeInterceptor() {}
   
   // Public --------------------------------------------------------
   
   public Object invoke(Invocation invocation)
      throws Throwable
   {
      // Is this create()?
      boolean create = invocation.getMethod().getName().equals("create");

      // Do we have a cached version?
      if (create && cached != null)
         return cached;

      // Not a cached create
      Object result = super.invoke(invocation);

      // We now have something to cache
      if (create)
         cached = result;

      return result;
   }
}
