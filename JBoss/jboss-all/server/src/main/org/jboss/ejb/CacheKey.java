/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.io.Externalizable;
import java.io.ObjectOutput;
import java.io.ObjectInput;
import java.io.IOException;
import java.lang.reflect.Method;
import java.rmi.MarshalledObject;

import org.jboss.logging.Logger;

/**
 * CacheKey is an encapsulation of both the PrimaryKey and a
 * cache specific key.
 *   
 * <p>This implementation is a safe implementation in the sense that it
 *    doesn't rely on the user supplied hashcode and equals.   It is also
 *    fast since the hashCode operation is pre-calculated.
 *
 * @see org.jboss.ejb.plugins.NoPassivationInstanceCache.java
 * @see org.jboss.ejb.plugins.EntityInstanceCache
 * @see org.jboss.ejb.plugins.EntityProxy
 * 
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @author <a href="Scott.Stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.21 $
 */
public class CacheKey
   implements Externalizable
{
   // Constants -----------------------------------------------------
   static final long serialVersionUID = -7108821554259950778L;
    
   // Attributes ----------------------------------------------------

   /**
    * The database primaryKey.
    * 
    * This primaryKey is used by:
    *
    * org.jboss.ejb.plugins.EntityInstanceCache.setKey() - to set the EntityEnterpriseContext id
    * org.jboss.ejb.plugins.jrmp.interfaces.EntityProxy.invoke():
    * - implementing Entity.toString() --> cacheKey.getId().toString()
    * - implementing Entity.hashCode() --> cacheKey.getId().hashCode()
    * - etc...
    * org.jboss.ejb.plugins.local.BaseLocalProxyFactory.EntityProxy.getId()
    */
   protected Object id;
   
   public Object getId()
   {
      return id;
   }
     
   /** The Marshalled Object representing the key */
   protected MarshalledObject mo;
    
   /** The Marshalled Object's hashcode */
   protected int hashCode;
    
   // Static --------------------------------------------------------  
    
   // Public --------------------------------------------------------
    
   public CacheKey()
   {
      // For externalization only
   }

   public CacheKey(Object id)
   {
      // why does this throw an error and not an IllegalArgumentException ?
      if (id == null) throw new Error("id may not be null");
         
      this.id = id;
      try
      {
         /* See if the key directly implements equals and hashCode. The
          *getDeclaredMethod method only returns method declared in the argument
          *class, not its superclasses.
         */
         try
         {
            Class[] equalsArgs = {Object.class};
            Method equals = id.getClass().getDeclaredMethod("equals", equalsArgs);
            Class[] hashCodeArgs = {};
            Method hash = id.getClass().getDeclaredMethod("hashCode", hashCodeArgs);
            // Both equals and hashCode are defined, use the id methods
            hashCode = id.hashCode();
         }
         catch(NoSuchMethodException ex)
         {
            // Rely on the MarshalledObject for equals and hashCode
            mo =  new MarshalledObject(id);
            // Precompute the hashCode (speed)
            hashCode = mo.hashCode();
         }
      }
      catch (Exception e)
      {
         Logger log = Logger.getLogger(getClass());
         log.error("failed to initialize, id="+id, e);
      }
   }

   // Z implementation ----------------------------------------------
    
   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------
    
   public void writeExternal(ObjectOutput out)
      throws IOException
   {
      out.writeObject(id);
      out.writeObject(mo);
      out.writeInt(hashCode);
   }
   
   public void readExternal(ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      id = in.readObject();
      mo = (MarshalledObject) in.readObject();
      hashCode = in.readInt();
   }

   // HashCode and Equals over write --------------------------------
    
   /**
    * these should be overwritten by extending Cache key
    * since they define what the cache does in the first place
    */
   public int hashCode()
   {
      // we default to the pK id
      return hashCode;
   }
    
   /** This method uses the id implementation of equals if the mo is
    *null since this indicates that the id class did implement equals.
    *If mo is not null, then the MarshalledObject equals is used to
    *compare keys based on their serialized form. Relying on the
    *serialized form does not always work.
    */
   public boolean equals(Object object)
   {
      boolean equals = false;
      if (object instanceof CacheKey)
      {
         CacheKey ckey = (CacheKey) object;
         Object key = ckey.id;
         // If mo is null, the id class implements equals
         if( mo == null )
            equals = id.equals(key);
         else
            equals = mo.equals(ckey.mo);
      }
      return equals;
   }

   public String toString()
   {
      return id.toString();
   }
    
   // Inner classes -------------------------------------------------
}
