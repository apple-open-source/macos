/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb;


/**
*   FastKey
* 
*   FastKey is a hack to enable fool proof and fast operation of caches for Entity
*   In the case of complex PK if a developer misses hash and equals the maps won't
*   properly work in jboss.  Here we provide an wrapper to the DB Key and the hash
*   is over-written so that we never miss a hit in cache and have constant speed.
*   
*   @see org.jboss.ejb.plugins.NoPassivationInstanceCache.java
*   @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
*   @version $Revision: 1.1 $
*/
public class FastKey
	extends CacheKey
	implements java.io.Externalizable
{
    // Constants -----------------------------------------------------
    
    // Attributes ----------------------------------------------------
  
  	protected Long fastKey;
	
    // Static --------------------------------------------------------  
    
    // The seed for the fastKey id
    // MF FIXME: I suspect this is weak, if somebody ask for these all the time (heavy server)
    // then a server restart will recieve requests from previous servers and miss these... 
    // Think more about it.
    private static long seedKey = System.currentTimeMillis();
    
    
    // Constructors --------------------------------------------------
    
	public FastKey() {
		
		// For externalization only 		
	}
	
	public FastKey(Object id) {
		super(id);
		// The FastKey is based on a counter
		this.fastKey = nextFastKey();
	}
    // Public --------------------------------------------------------
    
	public Object getPrimaryKey() {
		
		return id;
	}
    
    
    // Z implementation ----------------------------------------------
    
    // Package protected ---------------------------------------------
    
    // Protected -----------------------------------------------------
    
    protected Long nextFastKey()
    {
        //increment the timeStamp
        return new Long(seedKey++);
    }
    
    
   public void writeExternal(java.io.ObjectOutput out)
      throws java.io.IOException
   {
        out.writeObject(id);
       out.writeObject(fastKey);
   }
   
   public void readExternal(java.io.ObjectInput in)
      throws java.io.IOException, ClassNotFoundException
   {
        id = in.readObject();
       fastKey = (Long) in.readObject();
   }

    
    // Private -------------------------------------------------------
    
    // HashCode and Equals over write --------------------------------
    
    public int hashCode() {
        
        // the fastKey is always assigned
        return fastKey.intValue();
    }
    
    
    public boolean equals(Object object) {
        
        if (object instanceof FastKey) {
            
            return fastKey.equals(((FastKey) object).fastKey);
        }
        return false;
    }
    
    // Inner classes -------------------------------------------------
}

