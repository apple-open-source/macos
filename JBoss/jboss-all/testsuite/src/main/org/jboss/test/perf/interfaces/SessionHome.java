package org.jboss.test.perf.interfaces;
// SessionHome.java

public interface SessionHome extends javax.ejb.EJBHome {

  Session create(String entityName) 
    throws java.rmi.RemoteException, javax.ejb.CreateException;

}
