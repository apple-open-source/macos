/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.xa.test;

import java.rmi.*;


import javax.naming.Context;
import javax.naming.InitialContext;
import javax.ejb.DuplicateKeyException;
import javax.ejb.Handle;
import javax.ejb.EJBMetaData;
import javax.ejb.FinderException;

import java.util.Date;
import java.util.Properties;
import java.util.Collection;
import java.util.Iterator;
import java.util.Enumeration;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

import org.jboss.test.xa.interfaces.CantSeeDataException;
import org.jboss.test.xa.interfaces.XATest;
import org.jboss.test.xa.interfaces.XATestHome;

/* This needs lots more work to be an acceptable test case.  
Aside from needing an xa-capable database, we need an xa-test-service.xml
file to deploy the two ConnectionFactoryLoaders.
Then the printing needs to be changed to assertions.


*/

public class XAUnitTestCase
    extends JBossTestCase
{
   org.apache.log4j.Category log = getLog();
    public XAUnitTestCase(String name)
    {
        super(name);
    }

    public void testXABean() throws Exception {
        int test = 0;

        Context ctx = new InitialContext();

         // sed kicks ass
        System.out.print(++test+"- "+"Looking up the XATest home...");

        XATestHome home;

        try {
            home = (XATestHome) ctx.lookup("XATest");

            if (home == null) throw new Exception("No Home!");
            log.debug("OK");
        } catch (Exception e) {
             // sed kicks ass
            log.debug("Could not lookup the context:  the beans are probably not deployed");
            log.debug("Check the server trace for details");

            throw e;
        }

         // sed kicks ass
        System.out.print(++test+"- "+"Creating an the XATest bean...");
        XATest bean;
        try {
            bean = home.create();
            if(bean == null) throw new Exception("No Bean!");
            log.debug("OK");
        } catch (Exception e) {
             // sed kicks ass
            log.debug("Could not create the bean!");
            log.debug("Check the server trace for details");
            log.debug("failed", e);


            throw e;
        }

         // sed kicks ass
        System.out.print(++test+"- "+"Creating required tables...");
        try {
            bean.createTables();
            log.debug("OK");            
        }
        catch (Exception e) {
            log.debug("\nFailed to create tables");
            throw e;
        }
        
         // sed kicks ass
        System.out.print(++test+"- "+"Clearing any old data...");
        try {
            bean.clearData();
            log.debug("OK");
        } catch(Exception e) {
             // sed kicks ass
            log.debug("Could not clear the data: did you create the table in both data sources?");
            log.debug("CREATE TABLE XA_TEST(ID INTEGER NOT NULL PRIMARY KEY, DATA INTEGER NOT NULL)");

            throw e;
        }

         // sed kicks ass
        System.out.print(++test+"- "+"Testing DB connections...");
        try {
            bean.doWork();
            log.debug("OK");
        } catch(CantSeeDataException e) {
            log.debug("sort of worked.");
            log.debug(e.getMessage());
        } catch(Exception e) {
             // sed kicks ass
            log.debug("Error during DB test!");
            log.debug("Check the server trace for details");

            throw e;
        }
    }


   public static Test suite() throws Exception
   {
      return getDeploySetup(XAUnitTestCase.class, "xatest.jar");
   }

}
