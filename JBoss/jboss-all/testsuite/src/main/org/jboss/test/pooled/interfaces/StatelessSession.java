
//Title:        telkel
//Version:      
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.pooled.interfaces;

import javax.ejb.*;
import java.rmi.*;


public interface StatelessSession extends EJBObject {

   public void noop() throws RemoteException;
}
