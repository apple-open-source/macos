/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.ha.singleton;

import java.util.Stack;

import javax.management.Notification;

import org.jboss.ha.singleton.HASingletonSupport;


/**
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HASingletonSupportTester extends HASingletonSupport
{

  public Stack __invokationStack__ = new Stack();

  public boolean __isDRMMasterReplica__ = false;

  public boolean __isSingletonStarted__ = false;

  protected void setupPartition() throws Exception
  {
    __invokationStack__.push("setupPartition");
  }

  protected void registerRPCHandler()
  {
    __invokationStack__.push("registerRPCHandler");
  }

  protected void unregisterRPCHandler()
  {
    __invokationStack__.push("unregisterRPCHandler");
  }

  protected void registerDRMListener() throws Exception
  {
    __invokationStack__.push("registerDRMListener");
  }

  protected void unregisterDRMListener() throws Exception
  {
    __invokationStack__.push("unregisterDRMListener");
  }

  protected boolean isDRMMasterReplica()
  {
    __invokationStack__.push("isDRMMasterReplica");
    return __isDRMMasterReplica__;
  }

  public void callMethodOnPartition(String methodName, Object[] args)
    throws Exception
  {
    __invokationStack__.push("callMethodOnCluster:" + methodName);
  }

  public void startSingleton()
  {
    __invokationStack__.push("startSingleton");
  }

  public void stopSingleton()
  {
    __invokationStack__.push("stopSingleton");
  }

  protected void makeThisNodeMaster()
  {
    __invokationStack__.push("makeThisNodeMaster");
    super.makeThisNodeMaster();
  }


  public void sendNotification(Notification notification)
  {
    return;
  }

}
