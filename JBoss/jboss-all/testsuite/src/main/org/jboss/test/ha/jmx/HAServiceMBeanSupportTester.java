/* 
 * ====================================================================
 * This is Open Source Software, distributed
 * under the Apache Software License, Version 1.1
 * 
 * 
 *  This software  consists of voluntary contributions made  by many individuals
 *  on  behalf of the Apache Software  Foundation and was  originally created by
 *  Ivelin Ivanov <ivelin@apache.org>. For more  information on the Apache
 *  Software Foundation, please see <http://www.apache.org/>.
 */

package org.jboss.test.ha.jmx;

import java.util.Stack;

import javax.management.MalformedObjectNameException;
import javax.management.Notification;
import javax.management.ObjectName;

import org.jboss.ha.jmx.HAServiceMBeanSupport;

/**
 * 
 * @author  Ivelin Ivanov <ivelin@apache.org>
 *
 */
public class HAServiceMBeanSupportTester extends HAServiceMBeanSupport
{

  public Stack __invokationStack__ = new Stack();

  public boolean __isDRMMasterReplica__ = false;

  public boolean __isSingletonStarted__ = false;

  public boolean __shouldSendNotificationRemoteFail__ = false;

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

  protected void sendNotificationRemote(Notification notification)
    throws Exception
  {
    if (__shouldSendNotificationRemoteFail__)
      throw new Exception("simulated exception");
    __invokationStack__.push("sendNotificationRemote");
    __invokationStack__.push(notification);
  }

  protected void sendNotificationToLocalListeners(Notification notification)
  {
    __invokationStack__.push("sendNotificationToLocalListeners");
    __invokationStack__.push(notification);
  }

  public ObjectName getServiceName()
  {
    ObjectName oname = null;
    try
    {
      oname = new ObjectName("jboss.examples:name=HAServiceMBeanSupportTester");
    }
    catch (MalformedObjectNameException e)
    {
      // TODO Auto-generated catch block
      e.printStackTrace();
    }
    return oname;
  }

}
