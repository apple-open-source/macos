package org.jboss.test.exception;

import java.rmi.RemoteException;
import java.util.Collection;
import java.util.Iterator;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import org.apache.log4j.Category;

public class ExceptionTesterBean implements SessionBean
{
   private SessionContext ctx;

   public void ejbCreate() throws CreateException {
   }

   public void applicationExceptionInTx() throws ApplicationException {
      throw new ApplicationException("Application exception from within " +
            " an inherited transaction");
   }

   public void applicationErrorInTx() {
      throw new ApplicationError("Application error from within " +
            " an inherited transaction");
   }

   public void ejbExceptionInTx() {
      throw new EJBException("EJB exception from within " +
            " an inherited transaction");
   }

   public void runtimeExceptionInTx() {
      throw new RuntimeException("Runtime exception from within " +
            " an inherited transaction");
   }

   public void remoteExceptionInTx() throws RemoteException {
      throw new RemoteException("Remote exception from within " +
            " an inherited transaction");
   }

   public void applicationExceptionNewTx() throws ApplicationException {
      throw new ApplicationException("Application exception from within " +
            " a new container transaction");
   }

   public void applicationErrorNewTx() {
      throw new ApplicationError("Application error from within " +
            " an inherited transaction");
   }

   public void ejbExceptionNewTx() {
      throw new EJBException("EJB exception from within " +
            " a new container transaction");
   }

   public void runtimeExceptionNewTx() {
      throw new RuntimeException("Runtime exception from within " +
            " a new container transaction");
   }

   public void remoteExceptionNewTx() throws RemoteException {
      throw new RemoteException("Remote exception from within " +
            " a new container transaction");
   }

   public void applicationExceptionNoTx() throws ApplicationException {
      throw new ApplicationException("Application exception without " +
            " a transaction");
   }

   public void applicationErrorNoTx() {
      throw new ApplicationError("Application error from within " +
            " an inherited transaction");
   }

   public void ejbExceptionNoTx() {
      throw new EJBException("EJB exception without " +
            " a transaction");
   }

   public void runtimeExceptionNoTx() {
      throw new RuntimeException("Runtime exception without " +
            " a transaction");
   }

   public void remoteExceptionNoTx() throws RemoteException {
      throw new RemoteException("Remote exception without " +
            " a transaction");
   }

   public void setSessionContext(SessionContext ctx)
   {
      ctx = ctx;
   }

   public void ejbActivate() { }

   public void ejbPassivate() { }

   public void ejbRemove() { }
}
