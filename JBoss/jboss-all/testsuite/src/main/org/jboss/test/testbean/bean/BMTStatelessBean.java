package org.jboss.test.testbean.bean;

import java.rmi.*;
import javax.ejb.*;
import javax.naming.InitialContext;
import javax.naming.Context;
import org.jboss.test.testbean.interfaces.BMTStateful;
import javax.transaction.UserTransaction;
import javax.transaction.Status;


public class BMTStatelessBean implements SessionBean {
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
  private SessionContext sessionContext;

  public void ejbCreate() throws RemoteException, CreateException {
  log.debug("BMTStatelessBean.ejbCreate() called");
  }

  public void ejbActivate() throws RemoteException {
    log.debug("BMTStatelessBean.ejbActivate() called");
  }

  public void ejbPassivate() throws RemoteException {
      log.debug("BMTStatelessBean.ejbPassivate() called");
  }

  public void ejbRemove() throws RemoteException {
     log.debug("BMTStatelessBean.ejbRemove() called");
  }

  public void setSessionContext(SessionContext context) throws RemoteException {
    sessionContext = context;
  }

	public String txExists() throws RemoteException {
		String result = "";
		try {
	  		UserTransaction ut1 = sessionContext.getUserTransaction();
			result += "Got UserTransaction via sessionContext.getUserTransaction()\n";
		
			UserTransaction ut2 = (UserTransaction)new InitialContext().lookup("java:comp/UserTransaction");
			result += "Got UserTransaction via lookup(java:comp/UserTransaction)\n";
			return result;
		} catch (Exception e) {
	    	throw new RemoteException(e.getMessage());
		}
	}
		
		
	public String txCommit() throws RemoteException {
  	    try {
			UserTransaction tx = sessionContext.getUserTransaction();
		
			String result = "Got transaction : " + statusName(tx.getStatus()) + "\n";
			tx.begin();
			result += "tx.begin(): " + statusName(tx.getStatus()) + "\n";
			tx.commit();
			result += "tx.commit(): " + statusName(tx.getStatus()) + "\n";
		
			return result;
			
		} catch (Exception e) {
			log.debug("failed", e);
			throw new RemoteException(e.getMessage());
		}
		
	}
		
	public String txRollback() throws RemoteException {
  	    try {
			UserTransaction tx = sessionContext.getUserTransaction();
		
			String result = "Got transaction : " + statusName(tx.getStatus()) + "\n";
			tx.begin();
			result += "tx.begin(): " + statusName(tx.getStatus()) + "\n";
			tx.rollback();
			result += "tx.rollback(): " + statusName(tx.getStatus()) + "\n";
		
			return result;
			
		} catch (Exception e) {
			throw new RemoteException(e.getMessage());
		}
	}
		
	  

	// this should not be allowed by the container
	public String txBegin() throws RemoteException {
		try {
			UserTransaction tx = sessionContext.getUserTransaction();
		
			tx.begin();
			return "status: " + statusName(tx.getStatus());
		} catch (Exception e) {
			throw new RemoteException(e.getMessage());
		}
		
	}

	private String statusName(int s) {
		switch (s) {
			case Status.STATUS_ACTIVE: return "STATUS_ACTIVE";
			case Status.STATUS_COMMITTED: return "STATUS_COMMITED"; 
			case Status.STATUS_COMMITTING: return "STATUS_COMMITTING"; 
			case Status.STATUS_MARKED_ROLLBACK: return "STATUS_MARKED_ROLLBACK"; 
			case Status.STATUS_NO_TRANSACTION: return "STATUS_NO_TRANSACTION"; 
			case Status.STATUS_PREPARED: return "STATUS_PREPARED"; 
			case Status.STATUS_PREPARING: return "STATUS_PREPARING"; 
			case Status.STATUS_ROLLEDBACK: return "STATUS_ROLLEDBACK"; 
			case Status.STATUS_ROLLING_BACK: return "STATUS_ROLLING_BACK"; 
			case Status.STATUS_UNKNOWN: return "STATUS_UNKNOWN"; 
	   }
	   return "REALLY_UNKNOWN";
	}   

}
