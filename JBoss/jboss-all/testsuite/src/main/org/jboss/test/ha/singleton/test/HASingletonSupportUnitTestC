/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */


package org.jboss.test.ha.singleton.test;
 
import java.util.ArrayList;

import junit.framework.TestCase;

import org.jboss.test.ha.singleton.HASingletonSupportTester;


public class HASingletonSupportUnitTestCase extends TestCase
{

  private HASingletonSupportTester singletonSupportTester = null;

  public HASingletonSupportUnitTestCase(String testCaseName)
  {
    super(testCaseName);
  }


  public void setUp()
  {
    singletonSupportTester = new HASingletonSupportTester();
  }
  
  public void tearDown() 
  {
    singletonSupportTester = null;
  }
  
  public void testStartService() throws Exception
  {
    singletonSupportTester.start();

    // test that the correct start sequence was followed correctly  
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "registerDRMListener");  
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "registerRPCHandler");  
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "setupPartition");  
      
  }

  public void testStopService() throws Exception
  {
    singletonSupportTester.start();
    singletonSupportTester.stop();

    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "unregisterRPCHandler");  
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "unregisterDRMListener");  
    
  }
  
  public void testBecomeMasterNode() throws Exception
  {
    singletonSupportTester.start();
    
    // register DRM Listener is expected to call back
    singletonSupportTester.__isDRMMasterReplica__ = true;
    singletonSupportTester.partitionTopologyChanged( new ArrayList(2), 1);

    // test whether it was elected    
    assertTrue("expected to become master", singletonSupportTester.isMasterNode());
    
    // test whether the election sequence was followed correctly
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "startSingleton");  
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "callMethodOnCluster:_stopOldMaster");  
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "makeThisNodeMaster");      
  }
  
  public void testBecomeSlaveNodeWithAnotherMaster() throws Exception
  {
    singletonSupportTester.start();
    
    boolean savedIsMasterNode = singletonSupportTester.isMasterNode();
    
    // register DRM Listener is expected to call back
    singletonSupportTester.__isDRMMasterReplica__ = false;
    singletonSupportTester.partitionTopologyChanged(new ArrayList(2), 1);
    
    // this call back should not change the master/slave status
    assertEquals("expected to be still in old master/slave state", singletonSupportTester.isMasterNode(), savedIsMasterNode );
    
    // the new master is expected to call back
    singletonSupportTester._stopOldMaster();
    
    if (savedIsMasterNode)
    {
      assertEquals("this node was the old master, but method not invoked as expected",
        singletonSupportTester.__invokationStack__.pop(), "stopSingleton");  
    }
      
    // now it should be slave
    assertTrue("expected to be slave", !singletonSupportTester.isMasterNode());
            
  }

  public void testStopOnlyNode() throws Exception
  {
    singletonSupportTester.start();
    
    // register DRM Listener is expected to call back
    singletonSupportTester.__isDRMMasterReplica__ = true;
    singletonSupportTester.partitionTopologyChanged( new ArrayList(2), 1);

    // test whether it was elected for master    
    assertTrue("expected to become master", singletonSupportTester.isMasterNode());
    
    singletonSupportTester.stop();
    
    // register DRM Listener is expected to call back
    singletonSupportTester.__isDRMMasterReplica__ = false;
    // since the only node (this one) in the partition is now removed, the replicants list should be empty 
    singletonSupportTester.partitionTopologyChanged(new ArrayList(0), 1);
    
    assertTrue("expected to have made a call to _stopOldMater(), thus become slave", !singletonSupportTester.isMasterNode() );
    
    assertEquals("method not invoked as expected",
      singletonSupportTester.__invokationStack__.pop(), "stopSingleton");  
      
  }
  
}
