/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: FederatedService.java,v 1.2 2002/04/26 11:53:27 cgjung Exp $

package org.jboss.test.net.external;

import javax.ejb.EJBObject;
import javax.ejb.EJBHome;
import javax.ejb.CreateException;

import java.rmi.RemoteException;

/**
 * @version 	1.0
 * @author
 */
public interface FederatedService extends EJBObject {

	public String findAndTranslate(String searchTerm) throws Exception, RemoteException;
	
	public interface Home extends EJBHome {
		public FederatedService create() throws CreateException, RemoteException;
	}
	
}
