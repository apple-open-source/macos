/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.cmrtransaction.interfaces;

import java.rmi.RemoteException;

import javax.ejb.EJBHome;
import javax.ejb.CreateException;

/**
 * @author  B Stansberry brian_stansberry@wanconcepts.com
 */
public interface TreeFacadeHome extends EJBHome
{
    TreeFacade create() throws CreateException, RemoteException;


}
