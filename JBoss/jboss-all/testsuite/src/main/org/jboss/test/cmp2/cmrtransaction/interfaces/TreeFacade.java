/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.cmrtransaction.interfaces;

import java.rmi.RemoteException;

import javax.ejb.EJBObject;

/**
 * @author  B Stansberry brian_stansberry@wanconcepts.com
 */
public interface TreeFacade extends EJBObject
{
    void setup() throws RemoteException;

    void createNodes() throws RemoteException;

    void rearrangeNodes() throws RemoteException;

}
