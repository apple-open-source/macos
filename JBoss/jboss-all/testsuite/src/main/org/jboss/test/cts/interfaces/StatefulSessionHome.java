/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cts.interfaces;


import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;


/**
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.7 $
 */
public interface StatefulSessionHome
   extends EJBHome
{
   public StatefulSession create(String testName)
      throws RemoteException, CreateException;
}

