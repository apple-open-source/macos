package org.jboss.test.perf.interfaces;
// EntityHome.java

public interface EntityHome extends javax.ejb.EJBHome
{
  Entity create(int key, int value) 
    throws java.rmi.RemoteException, javax.ejb.CreateException;

  Entity findByPrimaryKey(EntityPK primaryKey) 
    throws java.rmi.RemoteException, javax.ejb.FinderException;
}
