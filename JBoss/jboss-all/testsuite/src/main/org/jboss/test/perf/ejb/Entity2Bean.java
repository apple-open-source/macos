package org.jboss.test.perf.ejb;
// EntityBean.java
import org.jboss.test.perf.interfaces.Entity2PK;

public class Entity2Bean implements javax.ejb.EntityBean
{
   private javax.ejb.EntityContext _context;
   private transient boolean isDirty;

   public int key1;
   public String key2;
   public Double key3;
   public int the_value;

   public int read()
   {
      setModified(false); // to avoid writing
      return the_value;
   }
   
   public void write(int the_value)
   {
      setModified(true); // to force writing
      this.the_value = the_value;
   }
   
   public Entity2PK ejbCreate(int key1, String key2, Double key3, int value) 
   {
      this.key1 = key1;
      this.key2 = key2;
      this.key3 = key3;
      this.the_value = value;
      return null;
   }
   
   public void ejbPostCreate(int key1, String key2, Double key3, int value) 
   {
   }
   
   public void ejbRemove()
   {
   }
   
   public void setEntityContext(javax.ejb.EntityContext context)
   {
      _context = context;
   }
   
   public void unsetEntityContext()
   {
      _context = null;
   }
   
   public void ejbActivate()
   {
   }
   
   public void ejbPassivate()
   {
   }
   
   public void ejbLoad()
   {
      // WL
      setModified(false); // to avoid writing
   }
   
   public void ejbStore()
   {
      // WL
      setModified(false); // to avoid writing
   }
   
   public String toString()
   {
      return "EntityBean[key=(" + key1 + ',' + key2 + ',' + key3 +  "), the_value=" + the_value +"]";
   }

   // WL
   public boolean isModified()
   {
      return isDirty;
   }
   // WL
   public void setModified(boolean flag)
   {
      isDirty = flag;
   }
   
   
}
