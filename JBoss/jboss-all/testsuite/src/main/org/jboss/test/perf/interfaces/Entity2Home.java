package org.jboss.test.perf.interfaces;
// EntityHome.java

public interface Entity2Home extends javax.ejb.EJBHome {

  Entity create(int key1, String key2, Double key3, int value) 
    throws java.rmi.RemoteException, javax.ejb.CreateException;

  Entity findByPrimaryKey(Entity2PK primaryKey) 
    throws java.rmi.RemoteException, javax.ejb.FinderException;

  java.util.Enumeration findInRange(int min, int max) 
    throws java.rmi.RemoteException, javax.ejb.FinderException;

}
