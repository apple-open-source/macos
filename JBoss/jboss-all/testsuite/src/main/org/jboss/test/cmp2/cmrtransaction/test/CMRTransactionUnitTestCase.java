/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.cmrtransaction.test;

import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import junit.framework.Test;
import org.jboss.test.JBossTestCase;
import org.jboss.test.cmp2.cmrtransaction.interfaces.TreeFacadeHome;
import org.jboss.test.cmp2.cmrtransaction.interfaces.TreeFacade;

/**
 * @author  B Stansberry brian_stansberry@wanconcepts.com
 */
public class CMRTransactionUnitTestCase extends JBossTestCase
{
    // -------------------------------------------------------------  Constants

    // -------------------------------------------------------  Instance Fields

    // ----------------------------------------------------------  Constructors

    public CMRTransactionUnitTestCase(String name)
    {
        super(name);
    }

    // --------------------------------------------------------  Public Methods

    public void testCMRTransaction() throws Exception
    {

        InitialContext ctx = getInitialContext();
        Object obj = ctx.lookup("cmrTransactionTest/TreeFacadeRemote");
        TreeFacadeHome home = (TreeFacadeHome)
                PortableRemoteObject.narrow(obj, TreeFacadeHome.class);
        TreeFacade facade = home.create();
        facade.setup();
        facade.createNodes();

        int waitTime = 0;

        CMRTransactionThread rearrange = new CMRTransactionThread(facade);
        rearrange.start();
        rearrange.join();

        if (rearrange.exception != null)
        {
            fail(rearrange.exception.getMessage());
        }

        assertTrue(rearrange.finished);
    }

    // --------------------------------------------------------  Static Methods

    public static Test suite() throws Exception
    {
        return getDeploySetup(CMRTransactionUnitTestCase.class, "cmp2-cmrtransaction.jar");
    }

    // ---------------------------------------------------------  Inner Classes

    class CMRTransactionThread extends Thread
    {
        boolean finished = false;
        Exception exception = null;
        TreeFacade treeFacade = null;

        CMRTransactionThread(TreeFacade facade)
        {
            treeFacade = facade;
        }

        public void run()
        {
            try
            {
                treeFacade.rearrangeNodes();
            }
            catch (Exception e)
            {
                exception = e;
            }
            finally
            {
                finished = true;
            }
        }
    }

}
