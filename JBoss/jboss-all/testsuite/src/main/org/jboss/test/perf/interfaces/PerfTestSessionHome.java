package org.jboss.test.perf.interfaces;

public interface PerfTestSessionHome extends javax.ejb.EJBHome
{
   PerfTestSession create() throws java.rmi.RemoteException, javax.ejb.CreateException;
}
