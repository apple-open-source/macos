package org.jboss.test.perf.interfaces;
// Session.java

public interface Session extends javax.ejb.EJBObject
{

  public void create(int low, int high) 
    throws java.rmi.RemoteException, javax.ejb.CreateException;
  
  public void remove(int low, int high) 
    throws java.rmi.RemoteException, javax.ejb.RemoveException;

  public void read(int id) throws java.rmi.RemoteException;

  public void read(int low, int high) throws java.rmi.RemoteException;

  public void write(int id) throws java.rmi.RemoteException;

  public void write(int low, int high) throws java.rmi.RemoteException;
}
