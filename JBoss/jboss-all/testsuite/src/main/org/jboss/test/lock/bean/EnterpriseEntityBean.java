package org.jboss.test.lock.bean;

import java.rmi.*;
import javax.ejb.*;

import org.jboss.test.lock.interfaces.EnterpriseEntityHome;
import org.jboss.test.lock.interfaces.EnterpriseEntity;

public class EnterpriseEntityBean
   implements EntityBean
{
    static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(EnterpriseEntityBean.class);
   
   private EntityContext entityContext;
   
   public String name;
   public String field;
   public EnterpriseEntity nextEntity;
   public String lastEntity = "UNKNOWN!!!!";
	
   public String ejbCreate(final String name)
      throws RemoteException, CreateException
   {
      this.name = name;
      return null;
   }
	
   public void ejbPostCreate(String name)
      throws RemoteException, CreateException
   {
      // empty
   }
	
   public void ejbActivate() throws RemoteException {
      // empty
   }
	
   public void ejbLoad() throws RemoteException {
      // empty
   }
	
   public void ejbPassivate() throws RemoteException {
      // empty
   }
	
   public void ejbRemove() throws RemoteException, RemoveException {
      // empty
   }
	
   public void ejbStore() throws RemoteException {
      // empty
   }
	
   public void setField(String field) throws RemoteException {
      //log.debug("Bean "+name+", setField("+field+") called");
      this.field = field;
   }
	
   public String getField() throws RemoteException {
      return field;
   }

   public void setAndCopyField(String field) throws RemoteException {
      //log.debug("Bean "+name+", setAndCopyField("+field+") called");
		
      System.out.println("setAndCopyField");
      setField(field);
      if (nextEntity == null)
      {
         System.out.println("nextEntity is null!!!!!!!!");
         System.out.println("lastEntity: " + lastEntity);
      }
      nextEntity.setField(field);
   }
	
   public void setNextEntity(String beanName) throws RemoteException {
      //log.debug("EntityBean.createEntity() called");
		
      try {
         System.out.println("setNextEntity: " + beanName);
         EJBObject ejbObject = entityContext.getEJBObject();
         EnterpriseEntityHome home =
            (EnterpriseEntityHome)entityContext.getEJBObject().getEJBHome();
			
         try {
            nextEntity = (EnterpriseEntity)home.findByPrimaryKey(beanName);
         } catch (FinderException e) {
            nextEntity = (EnterpriseEntity)home.create(beanName);
         }
		
      } catch(Exception e) {
         log.debug("failed", e);
         throw new RemoteException
            ("create entity did not work check messages");   
      }
   }
	
   public void setEntityContext(EntityContext context)
      throws RemoteException
   {
      entityContext = context;
   }
	
   public void unsetEntityContext() throws RemoteException {
      entityContext = null;
   }

   public void sleep(long time) throws InterruptedException {
      Thread.sleep(time);
   }
}
