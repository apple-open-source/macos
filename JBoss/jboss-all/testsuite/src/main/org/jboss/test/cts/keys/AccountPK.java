package org.jboss.test.cts.keys;

import java.io.Serializable;

/**
 * Class AccountPK
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.4.2.1 $
 */
public class AccountPK
   implements Serializable
{
   public String key;

   /**
    * Constructor AccountPK
    *
    *
    * @param key
    *
    */

   public AccountPK (String key)
   {
      this.key = key;
   }

   /**
    * Method getKey
    *
    *
    * @return
    *
    */

   public String getKey ()
   {
      return this.key;
   }

   /**
    * Method equals
    *
    *
    * @return
    *
    */
   public boolean equals( Object obj )
   { 
       Class cl = obj.getClass( );
       AccountPK pk = (AccountPK)obj;
       return ( (cl.isInstance(this)) && (this.key.trim().compareTo(pk.getKey().trim()) == 0) );
   }

   public String toString()
   {
      return key;
   }

   public int hashCode()
   {
      return key.hashCode();
   }
      
}

