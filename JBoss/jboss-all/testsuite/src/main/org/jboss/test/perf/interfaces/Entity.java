package org.jboss.test.perf.interfaces;
// Entity.java

public interface Entity extends javax.ejb.EJBObject {

  int read() throws java.rmi.RemoteException;
  
  void write(int value) throws java.rmi.RemoteException;
  
}
