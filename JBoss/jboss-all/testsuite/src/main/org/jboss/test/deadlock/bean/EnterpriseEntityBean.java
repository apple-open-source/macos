
//Title:        telkel
//Version:      
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.deadlock.bean;

import java.util.Arrays;

import javax.ejb.*;

import org.jboss.ejb.plugins.TxInterceptorCMT;
import org.jboss.test.deadlock.interfaces.BeanOrder;
import org.jboss.test.deadlock.interfaces.EnterpriseEntityLocalHome;
import org.jboss.test.deadlock.interfaces.EnterpriseEntityLocal;
import org.jboss.test.deadlock.interfaces.EnterpriseEntityHome;
import org.jboss.test.deadlock.interfaces.EnterpriseEntity;

public abstract class EnterpriseEntityBean implements EntityBean 
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
  private EntityContext entityContext;
  public int otherField = 0;

  public String ejbCreate(String name) throws CreateException {
      setName(name);
	   return null;
  }

  public void ejbPostCreate(String name) throws CreateException {

	   
	   EJBLocalObject ejbObject = entityContext.getEJBLocalObject();
	   
	   if (ejbObject == null) {
		   log.debug("******************************* NULL EJBOBJECT in ejbPostCreate");
	   }
	   else {
			log.debug("&&&&&&&&&&&&&&&& EJBObject found in ejbPostCreate id is "+ejbObject.getPrimaryKey());   
	   }

  }

  public void ejbActivate() {
  }

  public void ejbLoad() {
  }

  public void ejbPassivate() {

  }

  public void ejbRemove() throws RemoveException {
  }

  public void ejbStore() {
	  
  }

  public abstract String getName();

  public abstract void setName(String name);

  public String callBusinessMethodA() {

     return "EntityBean.callBusinessMethodA() called, my primaryKey is "+
            entityContext.getPrimaryKey().toString();
  }
  
  public String callBusinessMethodB() {

     
	 EJBObject ejbObject = entityContext.getEJBObject();
	 
	 if (ejbObject == null) 
	 	return "NULL EJBOBJECT";
	 
	 else 
	 	return ejbObject.toString();
  }
  
  
   public String callBusinessMethodB(String words) {
    
     
	 EJBObject ejbObject = entityContext.getEJBObject();
	 
	 if (ejbObject == null) 
	 	return "NULL EJBOBJECT";
	 
	 else 
	 	return ejbObject.toString()+ " words "+words;
  
	}

  public abstract void setOtherField(int value);
  
  public abstract int getOtherField();

  public abstract void setNext(EnterpriseEntityLocal next);
  
  public abstract EnterpriseEntityLocal getNext();

  public void callAnotherBean(BeanOrder beanOrder)
  {
     // End of the chain
     if (beanOrder.next == beanOrder.order.length-1)
        return;

     // Call the next in the chain
     try
     {
        EnterpriseEntityLocalHome home = (EnterpriseEntityLocalHome)entityContext.getEJBLocalObject().getEJBLocalHome();
        beanOrder.next++;
        EnterpriseEntityLocal nextBean = home.findByPrimaryKey(beanOrder.order[beanOrder.next]);
        try
        {
           nextBean.callAnotherBean(beanOrder);
        }
        finally
        {
           beanOrder.next--;
        }
     }
     catch (Exception e)
     {
        Exception a = TxInterceptorCMT.isADE(e);
        if (a == null)
        {
           log.error("Error next=" + beanOrder.next + " order=" + Arrays.asList(beanOrder.order), e);
           throw new EJBException("callAnotherBean failed " + e.toString());
        }
        else
        {
           throw new EJBException ("ADE", a);
        }
     }
  }
  
  public EnterpriseEntity createEntity(String newName) {

    EnterpriseEntity newBean;
    try{
		EJBObject ejbObject = entityContext.getEJBObject();
		if (ejbObject == null) 
		log.debug("************************** NULL EJBOBJECT");
		else
        log.debug("************************** OK EJBOBJECT");
		
		EnterpriseEntityHome home = (EnterpriseEntityHome)entityContext.getEJBObject().getEJBHome();
	    newBean = (EnterpriseEntity)home.create(newName);

    
	}catch(Exception e)
    {
		log.debug("failed", e);
        throw new EJBException("create entity did not work check messages");   
    }
     
     return newBean;
  }
  
  public void setEntityContext(EntityContext context) {
     entityContext = context;
  }

  public void unsetEntityContext() {
    entityContext = null;
  }
}
