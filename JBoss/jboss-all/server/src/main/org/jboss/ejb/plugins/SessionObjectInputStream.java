/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectStreamClass;
import java.io.IOException;
import java.lang.reflect.Proxy;

import javax.ejb.Handle;
import javax.ejb.HomeHandle;

import org.jboss.ejb.StatefulSessionEnterpriseContext;

/**
 * The SessionObjectInputStream is used to deserialize stateful session beans when they are activated
 *      
 * @see org.jboss.ejb.plugins.SessionObjectOutputStream
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard berg</a>
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.9 $
 */
public class SessionObjectInputStream
   extends ObjectInputStream
{
   private StatefulSessionEnterpriseContext ctx;
   private ClassLoader appCl;

   // Constructors -------------------------------------------------
   public SessionObjectInputStream(StatefulSessionEnterpriseContext ctx, InputStream in)
      throws IOException
   {
      super(in);
      enableResolveObject(true);
      
      this.ctx = ctx;
      
      // cache the application classloader
      appCl = Thread.currentThread().getContextClassLoader();
   }
      
   // ObjectInputStream overrides -----------------------------------
   protected Object resolveObject(Object obj)
      throws IOException
   {
      Object resolved = obj;

      // section 6.4.1 of the ejb1.1 specification states what must be taken care of 
      
      // ejb reference (remote interface) : resolve handle to EJB
      if (obj instanceof Handle)
         resolved = ((Handle)obj).getEJBObject();
      
      // ejb reference (home interface) : resolve handle to EJB Home
      else if (obj instanceof HomeHandle)
         resolved = ((HomeHandle)obj).getEJBHome();
      
      // naming context: the jnp implementation of contexts is serializable, do nothing

      else if( obj instanceof HandleWrapper )
      {
         HandleWrapper wrapper = (HandleWrapper) obj;
         try
         {
            resolved = wrapper.get();
         }
         catch(ClassNotFoundException e)
         {
            throw new IOException("Failed to find class: "+e.getMessage());
         }
      }

      else if (obj instanceof StatefulSessionBeanField)
      {
         byte type = ((StatefulSessionBeanField)obj).type; 
       
         // session context: recreate it
         if (type == StatefulSessionBeanField.SESSION_CONTEXT)          
            resolved = ctx.getSessionContext();

         // user transaction: restore it
         else if (type == StatefulSessionBeanField.USER_TRANSACTION) 
            resolved = ctx.getSessionContext().getUserTransaction();      
      }
      return resolved;
   }

   /** Override the ObjectInputStream implementation to use the application class loader
    */
   protected Class resolveClass(ObjectStreamClass v) throws IOException, ClassNotFoundException
   {
      try
      {
         // use the application classloader to resolve the class
         return appCl.loadClass(v.getName());
         
      } catch (ClassNotFoundException e) {
         // we should probably never get here
         return super.resolveClass(v);
      }
   }

   /** Override the ObjectInputStream implementation to use the application class loader
    */
   protected Class resolveProxyClass(String[] interfaces) throws IOException, ClassNotFoundException
   {
       Class clazz = null;
       Class[] ifaceClasses = new Class[interfaces.length];
       for(int i = 0; i < interfaces.length; i ++)
           ifaceClasses[i] = Class.forName(interfaces[i], false, appCl);
       try
       {
           clazz = Proxy.getProxyClass(appCl, ifaceClasses);
       }
       catch(IllegalArgumentException e)
       {
           throw new ClassNotFoundException("Failed to resolve proxy class", e);
       }
       return clazz;
   }

}

