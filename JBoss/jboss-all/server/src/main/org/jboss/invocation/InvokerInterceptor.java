/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.invocation;

import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.Invoker;

import org.jboss.proxy.Interceptor;

import org.jboss.util.id.GUID;

/**
 * A very simple implementation of it that branches to the local stuff.
 * 
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.2 $
 */
public class InvokerInterceptor
   extends Interceptor
   implements Externalizable
{
   /** Serial Version Identifier. @since 1.2 */
   private static final long serialVersionUID = 2548120545997920357L;

   /** The value of our local Invoker.ID to detect when we are local. */
   private GUID invokerID = Invoker.ID;

   /** Invoker to the remote JMX node. */
   protected Invoker remoteInvoker;

   /** Static references to local invokers. */
   protected static Invoker localInvoker; 

   /**
    * Get the local invoker reference, useful for optimization.
    */
   public static Invoker getLocal()
   {
      return localInvoker;
   }

   /**
    * Set the local invoker reference, useful for optimization.
    */
   public static void setLocal(Invoker invoker)
   {
      localInvoker = invoker;
   }

   /**
    * Exposed for externalization.
    */
   public InvokerInterceptor()
   {
      super();
   }

   /**
    * Returns wether we are local to the originating container or not. 
    */
   public boolean isLocal()
   {
      return invokerID.equals(Invoker.ID);
   }

   /**
    * The invocation on the delegate, calls the right invoker.  
    * Remote if we are remote, local if we are local. 
    */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      Object returnValue = null;
      InvocationContext ctx = invocation.getInvocationContext();
      // optimize if calling another bean in same server VM
      if ( isLocal() )
      {
         // The payload as is is good
         returnValue = localInvoker.invoke(invocation);
      }
      else
      {
         // The payload will go through marshalling at the invoker layer
         Invoker invoker = ctx.getInvoker();
         returnValue = invoker.invoke(invocation);
      }
      return returnValue;
   }

   /**
    * Externalize this instance.
    *
    * <p>
    * If this instance lives in a different VM than its container
    * invoker, the remote interface of the container invoker is
    * not externalized.
    */
   public void writeExternal(final ObjectOutput out)
      throws IOException
   {
      out.writeObject(invokerID);
   }

   /**
    * Un-externalize this instance.
    *
    * <p>
    * We check timestamps of the interfaces to see if the instance is in the original
    * VM of creation
    */
   public void readExternal(final ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      invokerID = (GUID)in.readObject();
   }
}
