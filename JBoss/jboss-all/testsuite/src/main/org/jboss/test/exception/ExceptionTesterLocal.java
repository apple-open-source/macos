package org.jboss.test.exception;

import javax.ejb.EJBLocalObject;
import javax.ejb.FinderException;

public interface ExceptionTesterLocal extends EJBLocalObject
{
   public void applicationExceptionInTx() throws ApplicationException;
   public void applicationErrorInTx();
   public void ejbExceptionInTx();
   public void runtimeExceptionInTx();
   
   public void applicationExceptionNewTx() throws ApplicationException;
   public void applicationErrorNewTx();
   public void ejbExceptionNewTx();
   public void runtimeExceptionNewTx();

   public void applicationExceptionNoTx() throws ApplicationException;
   public void applicationErrorNoTx();
   public void ejbExceptionNoTx();
   public void runtimeExceptionNoTx();
}
