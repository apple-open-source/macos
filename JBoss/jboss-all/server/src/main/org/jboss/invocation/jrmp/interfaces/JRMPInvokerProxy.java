/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.invocation.jrmp.interfaces;

import java.io.IOException;
import java.io.Externalizable;
import java.io.ObjectInput;
import java.io.ObjectOutput;
import java.rmi.ConnectException;
import java.rmi.MarshalledObject;
import java.rmi.NoSuchObjectException;
import java.rmi.ServerException;
import java.rmi.server.RemoteObject;
import java.rmi.server.RemoteStub;
import javax.transaction.TransactionRolledbackException;
import javax.transaction.SystemException;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.Invoker;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.tm.TransactionPropagationContextFactory;

/**
 * JRMPInvokerProxy, local to the proxy and is capable of delegating to
 * the JRMP implementations
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.7.2.5 $
 */
public class JRMPInvokerProxy
   implements Invoker, Externalizable
{
   /** Serial Version Identifier. @since 1.7.2.4 */
   private  static final long serialVersionUID = -3713605626489646730L;
   // Attributes ----------------------------------------------------

   // Invoker to the remote JMX node
   protected Invoker remoteInvoker;

   /**
    * Factory for transaction propagation contexts.
    *
    * @todo: marcf remove all transaction spill from here
    *
    * When set to a non-null value, it is used to get transaction
    * propagation contexts for remote method invocations.
    * If <code>null</code>, transactions are not propagated on
    * remote method invocations.
    */
   protected static TransactionPropagationContextFactory tpcFactory = null;

   //  @todo: MOVE TO TRANSACTION
   //
   // TPC factory
   public static void setTPCFactory(TransactionPropagationContextFactory tpcf) {
      tpcFactory = tpcf;
   }

   /**
    * max retries on a ConnectException.
    */
   public static int MAX_RETRIES = 10;

   /**
    * Exposed for externalization.
    */
   public JRMPInvokerProxy()
   {
      super();
   }

   /**
    * Create a new Proxy.
    *
    * @param container    The remote interface of the container invoker of the
    *                     container we proxy for.
    */
   public JRMPInvokerProxy(final Invoker remoteInvoker)
   {
      this.remoteInvoker = remoteInvoker;
   }

   /**
    * The name of of the server.
    */
   public String getServerHostName() throws Exception
   {
      return remoteInvoker.getServerHostName();
   }

   /**
    * ???
    *
    * @todo: MOVE TO TRANSACTION
    *
    * @return the transaction propagation context of the transaction
    *         associated with the current thread.
    *         Returns <code>null</code> if the transaction manager was never
    *         set, or if no transaction is associated with the current thread.
    */
   public Object getTransactionPropagationContext()
      throws SystemException
   {
      return (tpcFactory == null) ? null : tpcFactory.getTransactionPropagationContext();
   }

   /**
    * The invocation on the delegate, calls the right invoker.  Remote if we are remote,
    * local if we are local.
    * @todo Shouldn't we unwrap _ALL_ RemoteExceptions?
    */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      // We are going to go through a Remote invocation, switch to a Marshalled Invocation
      MarshalledInvocation mi = new MarshalledInvocation(invocation);

      // Set the transaction propagation context
      //  @todo: MOVE TO TRANSACTION
      mi.setTransactionPropagationContext(getTransactionPropagationContext());

      // RMI seems to make a connection per invocation.
      // If too many clients are making an invocation
      // at same time, ConnectionExceptions happen
      for (int i = 0; i < MAX_RETRIES; i++)
      {
         try
         {
            MarshalledObject result = (MarshalledObject) remoteInvoker.invoke(mi);
            return result.get();
         }
         catch (ConnectException ce)
         {
            if (i + 1 < MAX_RETRIES)
            {
               Thread.sleep(1);
               continue;
            }
            throw ce;
         }
         catch (ServerException ex)
         {
            // Suns RMI implementation wraps NoSuchObjectException in
            // a ServerException. We cannot have that if we want
            // to comply with the spec, so we unwrap here.
            if (ex.detail instanceof NoSuchObjectException)
            {
               throw (NoSuchObjectException) ex.detail;
            }
            if (ex.detail instanceof TransactionRolledbackException)
            {
               throw (TransactionRolledbackException) ex.detail;
            }
            /* Shouldn't we unwrap _all_ remote exceptions with this code?
               if (ex.detail instanceof RemoteException)
               {
               throw (RemoteException) ex.detail;
               }
            */
            throw ex;
         }
      }
      throw new Exception("Unreachable statement");
   }

   /**
    * Externalize this instance and handle obtaining the remoteInvoker stub
    */
   public void writeExternal(final ObjectOutput out)
      throws IOException
   {
      /** We need to handle obtaining the RemoteStub for the remoteInvoker
       * since this proxy may be serialized in contexts that are not JRMP
       * aware.
       */
      if( remoteInvoker instanceof RemoteStub )
      {
         out.writeObject(remoteInvoker);
      }
      else
      {
         Object replacement = RemoteObject.toStub(remoteInvoker);
         out.writeObject(replacement);
      }
   }

   /**
    * Un-externalize this instance.
    *
    */
   public void readExternal(final ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      remoteInvoker = (Invoker) in.readObject();
   }
}

