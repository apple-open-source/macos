/******************************************************
 * File: TxSessionHome.java
 * created 07-Sep-00 8:36:22 PM by Administrator
 */


package org.jboss.test.testbean.interfaces;

         
import javax.ejb.*;
import java.rmi.*;


public interface TxSessionHome extends EJBHome {
    
  public TxSession create() throws java.rmi.RemoteException, javax.ejb.CreateException;
} 

