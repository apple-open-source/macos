/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins;

import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.invocation.Invocation;


/**
* The instance interceptors role is to break entity creation into two 
* calls, one for ejbCreate and one for ejbPostCreate. The ejbCreate
* method is passed over the invokeHome chain, and ejbPostCreate is 
* passed over the invoke chain.
*    
* @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
* @version $Revision: 1.1.4.2 $
*/
public class EntityCreationInterceptor extends AbstractInterceptor
{
   public Object invokeHome(Invocation mi)
      throws Exception
   {
      // Invoke through interceptors
      Object retVal = getNext().invokeHome(mi);

      // Is the context now with an identity? 
      // This means that a create method was called, so invoke ejbPostCreate.
      EntityEnterpriseContext ctx = 
            (EntityEnterpriseContext) mi.getEnterpriseContext();
      
      if(ctx != null && ctx.getId() != null)
      {
         // copy from the context into the mi
         // interceptors down the chain look in the mi for the id not the ctx.
         mi.setId(ctx.getId());
         
         // invoke down the invoke chain
         // the final interceptor in EntityContainer will redirect this
         // call to postCreateEntity, which calls ejbPostCreate
         getNext().invoke(mi);
      }
      
      return retVal;
   }

   public Object invoke(Invocation mi)
      throws Exception
   {
      // nothing to see here... move along
      return getNext().invoke(mi);
   }
}

