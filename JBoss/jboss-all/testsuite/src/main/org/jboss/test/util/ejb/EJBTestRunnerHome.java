/*
 * JUnitEJB
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.util.ejb;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 * Remote home interface of the ejb test runner.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1.2.1 $
 */
public interface EJBTestRunnerHome extends EJBHome
{
   /**
    * Creates an ejb test runner.
    * @return a new EJBTestRunner
    */
   public EJBTestRunner create() throws RemoteException, CreateException;
}