package org.jboss.test.perf.ejb;

import javax.ejb.CreateException;
import javax.ejb.EntityContext;

import org.jboss.test.perf.interfaces.EntityPK;

public abstract class EntityBean implements javax.ejb.EntityBean
{
   private EntityContext context;
   private transient boolean isDirty;
   
   public abstract int getTheKey();
   public abstract void setTheKey(int theKey);
   public abstract int getTheValue();
   public abstract void setTheValue(int theValue);

   public int read()
   {
      setModified(false); // to avoid writing
      return getTheValue();
   }
   
   public void write(int theValue)
   {
      setModified(true); // to force writing
      setTheValue(theValue);
   }
   
   public EntityPK ejbCreate(int theKey, int theValue)
      throws CreateException
   {
      setTheKey(theKey);
      setTheValue(theValue);
      return null;
   }

   public void ejbPostCreate(int theKey, int theValue)
   {
   }

   public void ejbRemove()
   {
   }

   public void setEntityContext(EntityContext context)
   {
      this.context = context;
   }

   public void unsetEntityContext()
   {
      this.context = null;
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
   
   public String toString()
   {
      return "EntityBean[theKey=" + getTheKey() + ",theValue=" + getTheValue() +"]";
   }

   public boolean isModified()
   {
      return isDirty;
   }

   public void setModified(boolean flag)
   {
      isDirty = flag;
   }
   
}

