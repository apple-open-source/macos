/******************************************************
 * File: TxSession.java
 * created 07-Sep-00 8:31:50 PM by Administrator
 */


package org.jboss.test.testbean.interfaces;


import javax.ejb.EJBObject;
import java.rmi.RemoteException;

public interface TxSession extends EJBObject 
{

  public String txRequired() throws RemoteException;
  
  public String txRequiresNew() throws RemoteException;
   
  public String txSupports() throws RemoteException;
 
  public String txMandatory() throws RemoteException;
  
  public String txNever() throws RemoteException;
 
  public String txNotSupported() throws RemoteException;
  
  public String requiredToSupports() throws RemoteException;
  
  public String requiredToNotSupported() throws RemoteException;
  
  public String requiredToRequiresNew() throws RemoteException;
 
}
