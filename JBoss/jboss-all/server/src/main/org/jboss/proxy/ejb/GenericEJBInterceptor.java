/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.ejb;

import java.io.Externalizable;
import java.lang.reflect.Method;
import javax.ejb.EJBHome;
import javax.ejb.EJBObject;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationKey;
import org.jboss.proxy.Interceptor;

/**
 * Generic Proxy 
 *
 * These proxies are independent of the transportation protocol.  Their role 
 * is to take care of some of the local calls on the client (done in extension
 * like EJB) 
 *      
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.3.2.1 $
 */
public abstract class GenericEJBInterceptor
   extends Interceptor
   implements Externalizable
{
   /** Serial Version Identifier. @since 1.3 */
   private static final long serialVersionUID = 3844706474734439975L;

   // Static method references
   protected static final Method TO_STRING;
   protected static final Method HASH_CODE;
   protected static final Method EQUALS;
   protected static final Method GET_PRIMARY_KEY;
   protected static final Method GET_HANDLE;
   protected static final Method GET_EJB_HOME;
   protected static final Method IS_IDENTICAL;
   
   /** Initialize the static variables. */
   static
   {
      try
      { 
         // Get the methods from Object
         Class[] empty = {};
         Class type = Object.class;
         
         TO_STRING = type.getMethod("toString", empty);
         HASH_CODE = type.getMethod("hashCode", empty);
         EQUALS = type.getMethod("equals", new Class[] { type });
         
         // Get the methods from EJBObject
         type = EJBObject.class;
         
         GET_PRIMARY_KEY = type.getMethod("getPrimaryKey", empty);
         GET_HANDLE = type.getMethod("getHandle", empty);
         GET_EJB_HOME = type.getMethod("getEJBHome", empty);
         IS_IDENTICAL = type.getMethod("isIdentical", new Class[] { type });
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new ExceptionInInitializerError(e);
      }
   }
   
   /**
    *  A public, no-args constructor for externalization to work.
    */
   public GenericEJBInterceptor()
   {
      // For externalization to work
   }
   
   protected EJBHome getEJBHome(Invocation invocation) throws NamingException
   {
      InitialContext iniCtx = new InitialContext();
      String jndiName = (String)invocation.getInvocationContext().getValue(
            InvocationKey.JNDI_NAME);
      return (EJBHome) iniCtx.lookup(jndiName);
   }
}
  
