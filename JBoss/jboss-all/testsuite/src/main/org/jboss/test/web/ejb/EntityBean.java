package org.jboss.test.web.ejb;

import org.jboss.test.web.interfaces.EntityPK;

public class EntityBean implements javax.ejb.EntityBean
{
   private javax.ejb.EntityContext _context;
   private transient boolean isDirty;
   
   public int the_key;
   public int the_value;
   
   public EntityPK ejbCreate(int the_key, int the_value)
   {
      this.the_key = the_key;
      this.the_value = the_value;
      return null;
   }
   
   public void ejbPostCreate(int the_key, int the_value)
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
      setModified(false); // to avoid writing
   }
   
   public void ejbStore()
   {
      setModified(false); // to avoid writing
   }
   
   public boolean isModified()
   {
      return isDirty;
   }

   public void setModified(boolean flag)
   {
      isDirty = flag;
   }

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

   public String toString()
   {
      return "EntityBean[the_key=" + the_key + ",the_value=" + the_value +"]";
   }
}
