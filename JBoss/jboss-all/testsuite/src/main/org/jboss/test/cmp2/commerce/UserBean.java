package org.jboss.test.cmp2.commerce;

import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.FinderException;

public abstract class UserBean implements EntityBean {
   transient private EntityContext ctx;

   public String ejbCreate(String userId) throws CreateException {
      setUserId(userId);
      return null;
   }

   public void ejbPostCreate(String id) {
   }

   public abstract String getUserId();
   public abstract void setUserId(String userId);

	public abstract String getUserName();
	public abstract void setUserName(String name);

	public abstract String getEmail();
	public abstract void setEmail(String email);

	public abstract boolean getSendSpam();
	public abstract void setSendSpam(boolean sendSpam);

   public abstract Collection ejbSelectUserIds() throws FinderException;
   public Collection getUserIds() throws FinderException {
      return ejbSelectUserIds();
   }

   public void setEntityContext(EntityContext ctx) { this.ctx = ctx; }
   public void unsetEntityContext() { this.ctx = null; }
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbLoad() { }
   public void ejbStore() { }
   public void ejbRemove() { }
}


