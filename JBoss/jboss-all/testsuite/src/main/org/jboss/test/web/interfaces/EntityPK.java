package org.jboss.test.web.interfaces;
// EntityPK.java

import java.net.URL;
import java.security.ProtectionDomain;

public class EntityPK implements java.io.Serializable
{
   static org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(EntityPK.class);
   
   public int the_key;
   
   public EntityPK()
   {
   }
   
   public EntityPK(int the_key)
   {
      this.the_key = the_key;
   }
   
   public boolean equals(Object obj)
   {
      boolean equals = false;
      try
      {
         EntityPK key = (EntityPK) obj;
         equals = the_key == key.the_key;
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
      return the_key;
   }
   
   public String toString()
   {
      return "EntityPK[" + the_key + "]";
   }
   
}
