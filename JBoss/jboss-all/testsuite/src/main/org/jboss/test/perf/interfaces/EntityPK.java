package org.jboss.test.perf.interfaces;

import java.net.URL;
import java.security.ProtectionDomain;

import org.apache.log4j.Logger;

/**
 @author Scott.Stark@jboss.org
 @version $Revision: 1.6 $
 */
public class EntityPK implements java.io.Serializable
{
   static Logger log = Logger.getLogger(EntityPK.class);
   
   public int theKey;
   
   public EntityPK()
   {
   }
   
   public EntityPK(int theKey)
   {
      this.theKey = theKey;
   }
   
   public boolean equals(Object obj)
   {
      boolean equals = false;
      try
      {
         EntityPK key = (EntityPK) obj;
         equals = theKey == key.theKey;
      }
      catch(ClassCastException e)
      {
         log.debug("failed", e);
         // Find the codebase of obj
         ProtectionDomain pd0 = getClass().getProtectionDomain();
         URL loc0 = pd0.getCodeSource().getLocation();
         ProtectionDomain pd1 = obj.getClass().getProtectionDomain();
         URL loc1 = pd1.getCodeSource().getLocation();
         log.debug("PK0 location="+loc0);
         log.debug("PK0 loader="+getClass().getClassLoader());
         log.debug("PK1 location="+loc1);
         log.debug("PK1 loader="+obj.getClass().getClassLoader());
      }
      return equals;
   }
   public int hashCode()
   {
      return theKey;
   }

   public String toString()
   {
      return "EntityPK[" + theKey + "]";
   }
   
}

