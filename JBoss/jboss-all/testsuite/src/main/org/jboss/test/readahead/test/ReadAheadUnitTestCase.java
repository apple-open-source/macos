/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.readahead.test;

import javax.naming.Context;
import javax.naming.InitialContext;

import junit.framework.TestCase;
import junit.framework.Test;
import junit.framework.TestSuite;

import org.jboss.test.readahead.interfaces.CMPFindTestSessionHome;
import org.jboss.test.readahead.interfaces.CMPFindTestSessionRemote;

import org.jboss.test.JBossTestCase;

/**
 * TestCase driver for the readahead finder tests
 * 
 * @author <a href="mailto:danch@nvisia.com">danch (Dan Christopherson</a>
 * @version $Id: ReadAheadUnitTestCase.java,v 1.4 2002/01/29 22:00:05 d_jencks Exp $
 * 
 * Revision:
 */
public class ReadAheadUnitTestCase extends JBossTestCase {

   CMPFindTestSessionRemote rem = null;
   
   public ReadAheadUnitTestCase(String name) {
      super(name);
   }

   protected void tearDown() throws Exception {
      if (rem != null) {
         getLog().debug("Removing test data");
         rem.removeTestData();
         
         rem.remove();
         
         rem = null;
      }
   }
      
   protected void setUp()
      throws Exception
   {
      CMPFindTestSessionHome home = 
         (CMPFindTestSessionHome)getInitialContext().lookup("CMPFindTestSession");
      rem = home.create();
      
      rem.createTestData();
   }
   
   public void testFindAll() throws Exception {
      rem.testFinder();
   }
   
   public void testFindByCity() throws Exception {
      rem.testByCity();
   }
   
   public void testAddressByCity() throws Exception {
      rem.addressByCity();
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(ReadAheadUnitTestCase.class, "readahead.jar");
   }

}
