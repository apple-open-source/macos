package org.jboss.test.deadlock.interfaces;


import javax.ejb.*;
import java.rmi.*;



public interface EnterpriseEntity extends EJBObject {

  public String callBusinessMethodA() throws RemoteException;
  public String callBusinessMethodB() throws RemoteException;
  public String callBusinessMethodB(String words) throws RemoteException;
  public void setOtherField(int value) throws RemoteException;
  public int getOtherField() throws RemoteException;
  public void callAnotherBean(BeanOrder beanOrder) throws RemoteException;
  public EnterpriseEntity createEntity(String newName) throws RemoteException;

}
