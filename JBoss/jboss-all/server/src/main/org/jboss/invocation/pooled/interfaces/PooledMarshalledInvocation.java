/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.pooled.interfaces;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationType;
import org.jboss.invocation.MarshalledInvocation;
import org.jboss.invocation.MarshalledValue;

import javax.transaction.Transaction;
import java.io.IOException;
import java.lang.reflect.Method;
import java.security.Principal;
import java.util.HashMap;
import java.util.Iterator;

/**
 * The MarshalledInvocation is an invocation that travels.  As such it serializes
 * its payload because of lack of ClassLoader visibility.
 * As such it contains Marshalled data representing the byte[] of the Invocation object it extends
 * Besides handling the specifics of "marshalling" the payload, which could be done at the Invocation level
 * the Marshalled Invocation can hold optimization and needed code for distribution for example the
 * TransactionPropagationContext which is a serialization of the TX for distribution purposes as
 * well as the "hash" for the methods that we send, as opposed to sending Method objects.
 * Serialization "optimizations" should be coded here in the externalization implementation of the class
 *
 *   @see <related>
 *   @author  <a href="mailto:marc@jboss.org">Marc Fleury</a>
 *   @version $Revision: 1.1.2.1 $
 *   Revisions:
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>2001120 marc fleury:</b>
 *   <ul>
 *   <li> Initial check-in
 *   </ul>
 *   <p><b>20020113 Sacha Labourey:</b>
 *   <ul>
 *   <li> Make Externalizable calls (writeExternal) idempotent: until now,
 *  serialization on a MarshalledInvocation could only performed once (transaction was removed
 *  and METHOD type was modified). If another call was re-using the same object (in
 *  clustering for example), the call was making a ClassCastException because of
 *  the changed METHOD type in the Map that occured in the previous call to writeExternal)
 *   </ul>
 *   <p><b>20020911 Bill Burke:</b>
 *   <ul>
 *   <li> Optimize access to certain variables.  Avoid hash lookups.
 *   </ul>
 */
public class PooledMarshalledInvocation
        extends MarshalledInvocation
        implements java.io.Externalizable
{
   // Constants -----------------------------------------------------

   /** Serial Version Identifier. */
   // todo put in serialVersionID
   //static final long serialVersionUID = -718723094688127810L;

   private transient Transaction tx;
   private transient Object credential;
   private transient Principal principal;
   private transient Object enterpriseContext;
   private transient Object id;
   private transient PooledMarshalledValue pooledMarshalledArgs;


   // Constructors --------------------------------------------------
   public PooledMarshalledInvocation()
   {
      // For externalization to work
   }

   public PooledMarshalledInvocation(Invocation invocation)
   {
      this.payload = invocation.payload;
      this.as_is_payload = invocation.as_is_payload;
      this.method = invocation.getMethod();
      this.objectName = invocation.getObjectName();
      this.args = invocation.getArguments();
      this.invocationType = invocation.getType();
   }


   /*
   public MarshalledInvocation(Map payload)
   {
      super(payload);
   }

   public MarshalledInvocation(Map payload, Map as_is_payload)
   {
      super(payload);
      this.as_is_payload = as_is_payload;
   }
   */
   public PooledMarshalledInvocation(
           Object id,
           Method m,
           Object[] args,
           Transaction tx,
           Principal identity,
           Object credential)
   {
      super(id, m, args, tx, identity, credential);
   }
   // Public --------------------------------------------------------



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
      if (id == null) id = super.getId();
      return id;
   }

   public void setId(Object id)
   {
      super.setId(id);
      this.id = id;
   }

   public void setTransaction(Transaction tx)
   {
      super.setTransaction(tx);
     this.tx = tx;
   }

   public Transaction getTransaction()
   {
      if (tx == null) tx = super.getTransaction();
      return this.tx;
   }

   public Object getCredential()
   {
      if (credential == null) credential = super.getCredential();
      return credential;
   }

   public void setCredential(Object credential)
   {
      super.setCredential(credential);
      this.credential = credential;
   }

   public Principal getPrincipal()
   {
      if (principal == null) principal = super.getPrincipal();
      return principal;
   }

   public void setPrincipal(Principal principal)
   {
      super.setPrincipal(principal);
      this.principal = principal;
   }


   public Object[] getArguments()
   {
      if (this.args == null)
      {
         try
         {
            this.args = (Object[]) pooledMarshalledArgs.get();
         }
         catch (Exception e)
         {
            e.printStackTrace();
         }
      }
      return args;
   }

   // Externalizable implementation ---------------------------------
   public void writeExternal(java.io.ObjectOutput out)
           throws IOException
   {
      out.writeObject(invocationType);
      // FIXME marcf: the "specific" treatment of Transactions should be abstracted.
      // Write the TPC, not the local transaction
      out.writeObject(tpc);

      long methodHash = calculateHash(this.method);
      out.writeLong(methodHash);

      out.writeInt(((Integer)this.objectName).intValue());
      out.writeObject(new PooledMarshalledValue(this.args));


      // Write out payload hashmap
      // Don't use hashmap serialization to avoid not-needed data being
      // marshalled
      // The map contains only serialized representations of every other object
      // Everything else is possibly tied to classloaders that exist inside the
      // server but not in the generic JMX land. they will travel in the  payload
      // as MarshalledValue objects, see the Invocation getter logic
      //
      if (payload == null)
         out.writeInt(0);
      else
      {
         out.writeInt(payload.size());
         Iterator keys = payload.keySet().iterator();
         while (keys.hasNext())
         {
            Object currentKey = keys.next();

            // This code could be if (object.getClass().getName().startsWith("java")) then don't serialize.
            // Bench the above for speed.

            out.writeObject(currentKey);
            out.writeObject(new MarshalledValue(payload.get(currentKey)));
         }
      }

      // This map is "safe" as is
      //out.writeObject(as_is_payload);
      if (as_is_payload == null)
         out.writeInt(0);
      else
      {
         out.writeInt(as_is_payload.size());

         Iterator keys = as_is_payload.keySet().iterator();
         while (keys.hasNext())
         {
            Object currentKey = keys.next();
            out.writeObject(currentKey);
            out.writeObject(as_is_payload.get(currentKey));
         }
      }
   }

   public void readExternal(java.io.ObjectInput in)
           throws IOException, ClassNotFoundException
   {
      invocationType = (InvocationType)in.readObject();
      tpc = in.readObject();
      this.methodHash = in.readLong();

      this.objectName = new Integer(in.readInt());

      pooledMarshalledArgs = (PooledMarshalledValue) in.readObject();

      int payloadSize = in.readInt();
      if (payloadSize > 0)
      {
         payload = new HashMap();
         for (int i = 0; i < payloadSize; i++)
         {
            Object key = in.readObject();
            Object value = in.readObject();
            payload.put(key, value);
         }
      }

      int as_is_payloadSize = in.readInt();
      if (as_is_payloadSize > 0)
      {
         as_is_payload = new HashMap();
         for (int i = 0; i < as_is_payloadSize; i++)
         {
            Object key = in.readObject();
            Object value = in.readObject();
            as_is_payload.put(key, value);
         }
      }
   }
}
