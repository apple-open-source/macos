/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.server;


import java.util.Vector;
import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;

import org.apache.log4j.Level;
import org.apache.log4j.Logger;

import org.jgroups.Channel;
import org.jgroups.log.Trace;

import org.jboss.logging.util.LoggerWriter;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.ServiceMBean;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.w3c.dom.Attr;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

/** 
 *   Management Bean for Cluster HAPartitions.  It will start a JGroups
 *   channel and initialize the ReplicantManager and DistributedStateService.
 *
 *   @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 *   @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 *   @version $Revision: 1.17.2.11 $
 */
public class ClusterPartition
   extends ServiceMBeanSupport 
   implements ClusterPartitionMBean
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   
   protected String partitionName = org.jboss.metadata.ClusterConfigMetaData.DEFAULT_PARTITION;
   protected String jgProps = /*
       "UDP(mcast_addr=228.1.2.3;mcast_port=45566):" +
       "PING:" +
       "FD(timeout=5000):" +
       "VERIFY_SUSPECT(timeout=1500):" +
       "MERGE:" + 
       "NAKACK:" +
       "UNICAST(timeout=5000;min_wait_time=2000):" +
       "FRAG:" +
       "FLUSH:" +
       "GMS:" +
       "STATE_TRANSFER:" +
       "QUEUE";*/
      "UDP(mcast_addr=228.1.2.3;mcast_port=45566;ip_ttl=64;" +
      "mcast_send_buf_size=150000;mcast_recv_buf_size=80000):" +
      "PING(timeout=2000;num_initial_members=3):" +
      "MERGE2(min_interval=5000;max_interval=10000):" +
      "FD:" +
      "VERIFY_SUSPECT(timeout=1500):" +
      "pbcast.STABLE(desired_avg_gossip=20000):" +
      "pbcast.NAKACK(gc_lag=50;retransmit_timeout=300,600,1200,2400,4800):" +
      "UNICAST(timeout=5000):" +
      "FRAG(down_thread=false;up_thread=false):" +
      // "CAUSAL:" +
      "pbcast.GMS(join_timeout=5000;join_retry_timeout=2000;" +
      "shun=false;print_local_addr=true):" +
      "pbcast.STATE_TRANSFER";
   /*
   protected String jgProps = 
       "UDP:" +
       "PING:" +
       "FD(trace=true;timeout=5000):" +
       "VERIFY_SUSPECT(trace=false;timeout=1500):" +
       "MERGE:" + 
       "NAKACK(trace=true):" +
       "UNICAST(timeout=5000;min_wait_time=2000):" +
       "FRAG:" +
       "FLUSH:" +
       "GMS:" +
       "STATE_TRANSFER(trace=true):" +
       "QUEUE";
   */
   protected HAPartitionImpl partition;
   protected boolean deadlock_detection = false;
   protected org.jgroups.JChannel channel;

   protected String nodeName = null;

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------
   
   // ClusterPartitionMBean implementation ----------------------------------------------
   
   public String getPartitionName()
   {
      return partitionName;
   }

   public void setPartitionName(String newName)
   {
      partitionName = newName;
   }

   public String getPartitionProperties()
   {
      return jgProps;
   }

   public void setPartitionProperties(String newProps)
   {
      jgProps = newProps;
   }

   /** Convert a list of elements to the JG property string
    */
   public void setPartitionConfig(Element config)
   {
      StringBuffer buffer = new StringBuffer();
      NodeList stack = config.getChildNodes();
      int length = stack.getLength();
      for(int s = 0; s < length; s ++)
      {
         Node node = stack.item(s);
         if( node.getNodeType() != Node.ELEMENT_NODE )
            continue;

         Element tag = (Element) node;
         String protocol = tag.getTagName();
         buffer.append(protocol);
         NamedNodeMap attrs = tag.getAttributes();
         int attrLength = attrs.getLength();
         if( attrLength > 0 )
            buffer.append('(');
         for(int a = 0; a < attrLength; a ++)
         {
            Attr attr = (Attr) attrs.item(a);
            String name = attr.getName();
            String value = attr.getValue();
            buffer.append(name);
            buffer.append('=');
            buffer.append(value);
            if( a < attrLength-1 )
               buffer.append(';');
         }
         if( attrLength > 0 )
            buffer.append(')');
         buffer.append(':');
      }
      // Remove the trailing ':'
      buffer.setLength(buffer.length()-1);
      this.jgProps = buffer.toString();
      log.debug("Setting JGProps from xml to: "+jgProps);
   }

   /**
    * Uniquely identifies this node. MUST be unique accros the whole cluster!
    * Cannot be changed once the partition has been started
    */
   public String getNodeName()
   {
      return this.nodeName;
   }

   public void setNodeName(String node) throws Exception
   {
      if (this.getState() == ServiceMBean.CREATED ||
          this.getState() == ServiceMBean.STARTED ||
          this.getState() == ServiceMBean.STARTING)
      {
         throw new Exception ("Node name cannot be changed once the partition has been started");
      }
      else
      {
         this.nodeName = node;
      }
   }

   public boolean getDeadlockDetection()
   {
      return deadlock_detection;
   }

   public void setDeadlockDetection(boolean doit)
   {
      deadlock_detection = doit;
   }

   protected ObjectName getObjectName(MBeanServer server, ObjectName name)
      throws MalformedObjectNameException
   {
      return name == null ? OBJECT_NAME : name;
   }
   
   public HAPartition getHAPartition ()
   {
      return this.partition;      
   }

   /** Return the list of member nodes that built from the current view
    * @return A Vector Strings representing the host:port values of the nodes
    */
   public Vector getCurrentView()
   {
      return partition.getCurrentView();
   }

   // ServiceMBeanSupport overrides ---------------------------------------------------
   
   public String getName()
   {
      return partitionName;
   }


   protected void createService()
      throws Exception
   {
      log.debug("Creating JGroups JChannel");

      this.channel = new org.jgroups.JChannel(jgProps);
      channel.setOpt(Channel.GET_STATE_EVENTS, new Boolean(true));	    
      channel.setOpt(Channel.AUTO_RECONNECT, new Boolean(true));
      channel.setOpt(Channel.AUTO_GETSTATE, new Boolean(true));

      // Force JGroups to use log4j
      Logger category = Logger.getLogger("org.javagroups."+partitionName);
      Level priority = category.getEffectiveLevel();
      /* Set the JGroups Trace level based on the log4j Level. The mapping
      is:
      Log4j Level    JGroups Trace
      TRACE          DEBUG
      DEBUG          ERROR
      INFO           ERROR
      ERROR          ERROR
      WARN           WARN
      FATAL          FATAL

      Neither the JGroups Trace.TEST level or Trace.INFO is used as its too
      verbose for our notion of DEBUG.
      */
      int traceLevel = Trace.INFO;
      if( priority.toInt() < Level.DEBUG.toInt() )
         traceLevel = Trace.DEBUG;
      else if ( priority == Level.DEBUG )
         traceLevel = Trace.ERROR;
      else if ( priority == Level.INFO )
         traceLevel = Trace.ERROR;
      else if ( priority == Level.ERROR )
         traceLevel = Trace.ERROR;
      else if ( priority == Level.WARN )
         traceLevel = Trace.WARN;
      else if ( priority == Level.FATAL )
         traceLevel = Trace.FATAL;
      LoggerWriter log4jWriter = new LoggerWriter(category, priority);
      Trace.setDefaultOutput(traceLevel, log4jWriter);
      log.info("Set the JGroups logging to log4j with category:"+category
         +", priority: "+priority+", JGroupsLevel: "+traceLevel);
      log.debug("Creating HAPartition");
      partition = new HAPartitionImpl(partitionName, channel, deadlock_detection, getServer());
      log.debug("Initing HAPartition: " + partition);
      partition.init();
      log.debug("HAPartition initialized");
   }

   protected void startService() 
      throws Exception
   {
      // We push the independant name in the protocol stack
      // before it is connected to the cluster
      //
      if (this.nodeName == null || "".equals(this.nodeName))
         this.nodeName = generateUniqueNodeName ();

      java.util.HashMap staticNodeName = new java.util.HashMap();
      staticNodeName.put("additional_data", this.nodeName.getBytes());
      this.channel.down(new org.jgroups.Event(org.jgroups.Event.CONFIG, staticNodeName));
      this.channel.getProtocolStack().flushEvents(); // temporary fix for JG bug (808170) TODO: REMOVE ONCE JGROUPS IS FIXED

      log.debug("Starting ClusterPartition: " + partitionName);
      channel.connect(partitionName);
      
      log.info("Starting channel");
      partition.startPartition();

      log.info("Started ClusterPartition: " + partitionName);
   }
   
   protected void stopService() throws Exception
   {
      log.debug("Stopping ClusterPartition: " + partitionName);
      partition.closePartition();
      log.info("Stopped ClusterPartition: " + partitionName);
   }

   protected String generateUniqueNodeName () throws Exception
   {
      // we first try to find a simple meaningful name:
      // 1st) "local-IP:JNDI_PORT" if JNDI is running on this machine
      // 2nd) "local-IP:JMV_GUID" otherwise
      // 3rd) return a fully GUID-based representation
      //

      // Before anything we determine the local host IP (and NOT name as this could be
      // resolved differently by other nodes...)

      String hostIP = null;
      try
      {
         hostIP = java.net.InetAddress.getLocalHost().getHostAddress();
      }
      catch (java.net.UnknownHostException e)
      {
         log.debug ("unable to create a GUID for this cluster, check network configuration is correctly setup (getLocalHost has returned an exception)");
         log.debug ("using a full GUID strategy");
         return new java.rmi.dgc.VMID().toString();
      }

      // 1st: is JNDI up and running?
      //
      try
      {
         javax.management.AttributeList al =
            this.server.getAttributes(org.jboss.naming.NamingServiceMBean.OBJECT_NAME,
                                      new String[] {"State", "Port"});

         int status = ((Integer)((javax.management.Attribute)al.get(0)).getValue()).intValue();
         if (status == ServiceMBean.STARTED)
         {
            // we can proceed with the JNDI trick!
            int port = ((Integer)((javax.management.Attribute)al.get(1)).getValue()).intValue();
            return hostIP + ":" + port;
         }
         else
         {
            log.debug("JNDI has been found but the service wasn't started so we cannot " +
                      "be entirely sure we are the only one that wants to use this PORT " +
                      "as a GUID on this host.");
         }

      }
      catch (javax.management.InstanceNotFoundException e)
      {
         log.debug ("JNDI not running here, cannot use this strategy to find a node GUID for the cluster");
      }
      catch (javax.management.ReflectionException e)
      {
         log.debug ("JNDI querying has returned an exception, cannot use this strategy to find a node GUID for the cluster");
      }

      // 2nd: host-GUID strategy
      //
      String uid = new java.rmi.server.UID().toString();
      return hostIP + ":" + uid;
   }

   public String showHistory ()
   {
      StringBuffer buff = new StringBuffer();
      Vector data = new Vector (this.partition.history);
      for (java.util.Iterator row = data.iterator(); row.hasNext();)
      {
         String info = (String) row.next();
         buff.append(info).append("\n");
      }
      return buff.toString();
   }

   public String showHistoryAsXML ()
   {
      StringBuffer buff = new StringBuffer();
      buff.append("<events>\n");
      Vector data = new Vector (this.partition.history);
      for (java.util.Iterator row = data.iterator(); row.hasNext();)
      {
         buff.append("   <event>\n      ");
         String info = (String) row.next();
         buff.append(info);
         buff.append("\n   </event>\n");
      }
      buff.append("</events>\n");
      return buff.toString();
   }
}
