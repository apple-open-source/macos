/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ha.framework.server.util;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.Vector;
import java.io.IOException;
import javax.naming.InitialContext;
import javax.management.ObjectName;

import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.interfaces.HAPartition.AsynchHAMembershipListener;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.logging.Logger;
import org.apache.log4j.MDC;

/** A cluster parition membership monitor. It can be used to view how
 the nodes in a cluster are seeing the topology change using either email or
 a centralized log server.

 To use this to send email on change notifications use the following log4j.xml
 fragments:

  <appender name="SMTP" class="org.apache.log4j.net.SMTPAppender">
    <param name="To" value="admin@dot.com"/>
    <param name="From" value="cluster-monitor@dot.com"/>
    <param name="Subject" value="JBoss Cluster Changes"/>
    <param name="SMTPHost" value="mailhost"/>
    <param name="BufferSize" value="8"/>
    <param name="EvaluatorClass"
      value="org.jboss.logging.appender.RegexEventEvaluator" />
    <layout class="org.apache.log4j.PatternLayout">
      <param name="ConversionPattern" value="[%d{ABSOLUTE},%c{1}] %m%n"/>
    </layout>
  </appender>

  <category name="org.jboss.ha.framework.server.util.TopologyMonitorService.membershipChanged">
    <priority value="DEBUG" />
    <appender-ref ref="SMTP"/>
  </category>

 You can also have this service notify another MBean of the change to perform
 arbitrary checks by specifying the MBean name as the TriggerServiceName
 attribute value. This MBean must have an operation with the following
 signature:
<pre>
   param: removed ArrayList<AddressPort> of nodes that were removed
   param: added ArrayList<AddressPort> of nodes that were added
   param: members ArrayList<AddressPort> of nodes currently in the cluster
   param: logCategoryName the log4j category name used by the
      TopologyMonitorService. This should be used for logging to integrate with
      the TopologyMonitorService output.
   public void membershipChanged(ArrayList deadMembers, ArrayList newMembers,
      ArrayList allMembers, String logCategoryName)
</pre>

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.4.2 $
 */
public class TopologyMonitorService extends ServiceMBeanSupport
   implements TopologyMonitorServiceMBean, AsynchHAMembershipListener
{
   private static final String CHANGE_NAME = TopologyMonitorService.class.getName()
         + ".membershipChanged";
   private static Logger changeLog = Logger.getLogger(CHANGE_NAME);
   private String partitionName = "DefaultPartition";
   private HAPartition partition;
   private String hostname;
   private ObjectName triggerServiceName;

   public TopologyMonitorService()
   {
   }

// --- Begin ServiceMBeanSupport overriden methods
   protected void startService() throws Exception
   {
      InitialContext ctx = new InitialContext();
      String partitionJndiName = "/HAPartition/" + partitionName;
      partition = (HAPartition) ctx.lookup(partitionJndiName);
      // Register as a listener of cluster membership changes
      partition.registerMembershipListener(this);
      log.info("Registered as MembershipListener");
      try
      {
         hostname = InetAddress.getLocalHost().getHostName();
      }
      catch(IOException e)
      {
         log.warn("Failed to lookup local hostname", e);
         hostname = "<unknown>";
      }
   }

   protected void stopService() throws Exception
   {
      partition.unregisterMembershipListener(this);
   }
// --- End ServiceMBeanSupport overriden methods

// --- Begin TopologyMonitorServiceMBean interface methods
   public String getPartitionName()
   {
      return partitionName;
   }
   public void setPartitionName(String name)
   {
      this.partitionName = name;
   }

   public ObjectName getTriggerServiceName()
   {
      return triggerServiceName;
   }
   public void setTriggerServiceName(ObjectName triggerServiceName)
   {
      this.triggerServiceName = triggerServiceName;
   }

   public Vector getClusterNodes()
   {
      Vector view = null;
      try
      {
         InitialContext ctx = new InitialContext();
         String jndiName = "/HAPartition/" + partitionName;
         HAPartition partition = (HAPartition) ctx.lookup(jndiName);
         view = partition.getCurrentView();
      }
      catch(Exception e)
      {
         log.error("Failed to access HAPartition state", e);
      }
      return view;
   }

// --- End TopologyMonitorServiceMBean interface methods

// --- Begin HAMembershipListener interface methods
   /** Called when a new partition topology occurs.
    * @param deadMembers A list of nodes that have died since the previous view
    * @param newMembers A list of nodes that have joined the partition since
    * the previous view
    * @param allMembers A list of nodes that built the current view
    */
   public void membershipChanged(final Vector deadMembers, final Vector newMembers,
         final Vector allMembers)
   {
      MDC.put("RegexEventEvaluator", "End membershipChange.*");
      ArrayList removed = new ArrayList();
      ArrayList added = new ArrayList();
      ArrayList members = new ArrayList();
      changeLog.info("Begin membershipChanged info, hostname="+hostname);
      changeLog.info("DeadMembers: size="+deadMembers.size());
      for(int m = 0; m < deadMembers.size(); m ++)
      {
         AddressPort addrInfo = getMemberAddress(deadMembers.get(m));
         removed.add(addrInfo);
         changeLog.info(addrInfo);
      }
      changeLog.info("NewMembers: size="+newMembers.size());
      for(int m = 0; m < newMembers.size(); m ++)
      {
         AddressPort addrInfo = getMemberAddress(newMembers.get(m));
         added.add(addrInfo);
         changeLog.info(addrInfo);
      }
      changeLog.info("AllMembers: size="+allMembers.size());
      for(int m = 0; m < allMembers.size(); m ++)
      {
         AddressPort addrInfo = getMemberAddress(allMembers.get(m));
         members.add(addrInfo);
         changeLog.info(addrInfo);
      }
      // Notify the trigger MBean
      if( triggerServiceName != null )
      {
         changeLog.info("Invoking trigger service: "+triggerServiceName);
         try
         {
            Object[] params = {removed, added, members, CHANGE_NAME};
            String[] sig = {"java.util.ArrayList", "java.util.ArrayList",
               "java.util.ArrayList", "java.lang.String"};
            server.invoke(triggerServiceName, "membershipChanged", params, sig);
         }
         catch(Throwable t)
         {
            changeLog.error("Failed to notify trigger service: "+triggerServiceName, t);
            log.debug("Failed to notify trigger service: "+triggerServiceName, t);
         }
      }
      changeLog.info("End membershipChanged info, hostname="+hostname);
      MDC.remove("RegexEventEvaluator");
   }
// --- End HAMembershipListener interface methods

   /** Use reflection to access the address InetAddress and port if they exist
    * in the Address implementation
    */
   private AddressPort getMemberAddress(Object addr)
   {
      AddressPort info = null;
      try
      {
         org.jboss.ha.framework.interfaces.ClusterNode node =
               (org.jboss.ha.framework.interfaces.ClusterNode)addr;

         InetAddress inetAddr = node.getOriginalJGAddress().getIpAddress();
         Integer port = new Integer(node.getOriginalJGAddress().getPort());
         info = new AddressPort(inetAddr, port);
      }
      catch(Exception e)
      {
         log.warn("Failed to obtain InetAddress/port from addr: "+addr, e);
      }
      return info;
   }

   public static class AddressPort
   {
      InetAddress addr;
      Integer port;
      AddressPort(InetAddress addr, Integer port)
      {
         this.addr = addr;
         this.port = port;
      }

      public Integer getPort()
      {
         return port;
      }
      public InetAddress getInetAddress()
      {
         return addr;
      }
      public String getHostAddress()
      {
         return addr.getHostAddress();
      }
      public String getHostName()
      {
         return addr.getHostName();
      }
      public String toString()
      {
         return "{host("+addr+"), port("+port+")}";
      }
   }
}
