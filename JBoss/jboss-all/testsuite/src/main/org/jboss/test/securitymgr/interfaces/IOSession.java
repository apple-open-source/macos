package org.jboss.test.securitymgr.interfaces;

import java.io.IOException;
import java.rmi.RemoteException;
import javax.ejb.EJBObject;

public interface IOSession extends EJBObject
{
   public String read(String path) throws IOException, RemoteException;
   public void write(String path) throws IOException, RemoteException;
   public void listen(int port) throws IOException;
   public void connect(String host, int port) throws IOException;
   public void createClassLoader() throws RemoteException;
   public void getContextClassLoader() throws RemoteException;
   public void setContextClassLoader() throws RemoteException;
   public void renameThread() throws RemoteException;
   public void useReflection() throws RemoteException;
   public void loadLibrary() throws RemoteException;
   public void createSecurityMgr() throws RemoteException;
   public void changeSystemOut() throws RemoteException;
   public void changeSystemErr() throws RemoteException;
   public void systemExit(int status) throws RemoteException;
}
