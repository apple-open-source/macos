
//Title:        telkel
//Version:      
//Copyright:    Copyright (c) 1999
//Author:       Marc Fleury
//Company:      telkel
//Description:  Your description

package org.jboss.test.testbean.interfaces;

import javax.ejb.*;
import java.rmi.*;

public interface StatefulSessionHome extends EJBHome {
	
	public StatefulSession create() throws RemoteException, CreateException;
	
	public StatefulSession create(String name) throws RemoteException, CreateException;
	
	public StatefulSession create(String name, String address) throws RemoteException, CreateException;
	
	public StatefulSession createMETHOD(String name, String address) throws RemoteException, CreateException;
} 
