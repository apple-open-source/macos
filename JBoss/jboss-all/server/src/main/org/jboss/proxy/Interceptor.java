/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy;

import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;
import org.jboss.invocation.Invocation;

/**
 * The base class for all interceptors.
 * 
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.2.2.2 $
 */
public abstract class Interceptor
   implements Externalizable
{
   /** The serialVersionUID. @since 1.2 */
   private static final long serialVersionUID = 4358098404672505200L;

   /** The next interceptor in the chain. */
   protected Interceptor nextInterceptor;
 
   /**
    * Set the next interceptor in the chain.
    * 
    * <p>
    * String together the interceptors
    * We return the passed interceptor to allow for 
    * interceptor1.setNext(interceptor2).setNext(interceptor3)... constructs.
    */
   public Interceptor setNext(final Interceptor interceptor) {
      // assert interceptor != null
      nextInterceptor = interceptor;
      return interceptor;
   }
   
   public Interceptor getNext() {
      return nextInterceptor;
   }

   public abstract Object invoke(Invocation mi) throws Throwable;
   
   /**
    * Writes the next interceptor.
    */
   public void writeExternal(final ObjectOutput out)
      throws IOException
   {
      out.writeObject(nextInterceptor);
   }

   /**
    * Reads the next interceptor.
    */
   public void readExternal(final ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      nextInterceptor = (Interceptor)in.readObject();
   }
}
