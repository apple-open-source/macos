
//Title:        telkel
//Version:      
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.testbean.bean;

import java.rmi.*;
import javax.ejb.*;

public class StatefulSessionBean implements SessionBean {
  public static org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(StatefulSessionBean.class);
  private SessionContext sessionContext;
  public String name;

  public void ejbCreate() throws RemoteException, CreateException {
	
	  log.debug("StatefulSessionBean.ejbCreate() called");
	  this.name= "noname";
  }
  
  public void ejbCreate(String name) throws RemoteException, CreateException {
      log.debug("StatefulSessionBean.ejbCreate("+name+") called");
      this.name = name;
  }

  public void ejbCreate(String name, String address) throws RemoteException, CreateException {
      log.debug("StatefulSessionBean.ejbCreate("+name+"@"+address+") called");
      this.name = name;
  }

  public void ejbCreateMETHOD(String name, String address) throws RemoteException, CreateException {
      log.debug("StatefulSessionBean.ejbCreateMETHOD("+name+"@"+address+") called");
      this.name = name;
  }

  public void ejbActivate() throws RemoteException {
      log.debug("StatefulSessionBean.ejbActivate() called");
  }

  public void ejbPassivate() throws RemoteException {
     log.debug("StatefulSessionBean.ejbPassivate() called");
  }

  public void ejbRemove() throws RemoteException {
     log.debug("StatefulSessionBean.ejbRemove() called");
  }

  public String callBusinessMethodA() {
     log.debug("StatefulSessionBean.callBusinessMethodA() called");
     return "I was created with Stateful String "+name;
  }

	public String callBusinessMethodB() {
		 log.debug("StatefulSessionBean.callBusinessMethodB() called");
         // Check that my EJBObject is there
		 EJBObject ejbObject = sessionContext.getEJBObject();
		 if (ejbObject == null) {
		 	 return "ISNULL:NOT FOUND!!!!!";
		
		 }
		 else {
		 	return "OK ejbObject is "+ejbObject.toString();
			
		 }			 
  }
  
  
  public String callBusinessMethodB(String words) {
  	 log.debug("StatefulSessionBean.callBusinessMethodB(String) called");
         // Check that my EJBObject is there
		 EJBObject ejbObject = sessionContext.getEJBObject();
		 if (ejbObject == null) {
		 	 return "ISNULL:NOT FOUND!!!!!";
		
		 }
		 else {
		 	return "OK ejbObject is "+ejbObject.toString()+" words "+words;
			
		 }			 
  
  }
  
  
  public void setSessionContext(SessionContext context) throws RemoteException {
     log.debug("StatefulSessionBean.setSessionContext("+context+") called");
     sessionContext = context;
  }
} 
