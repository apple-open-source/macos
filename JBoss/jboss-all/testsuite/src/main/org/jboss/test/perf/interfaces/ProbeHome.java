package org.jboss.test.perf.interfaces;

public interface ProbeHome extends javax.ejb.EJBHome
{
   Probe create() throws java.rmi.RemoteException, javax.ejb.CreateException;
}
