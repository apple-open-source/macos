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

import org.javagroups.Channel;
import org.javagroups.log.Trace;

import org.jboss.logging.util.LoggerWriter;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.w3c.dom.Attr;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

/** 
 *   Management Bean for Cluster HAPartitions.  It will start a JavaGroups
 *   channel and initialize the ReplicantManager and DistributedStateService.
 *
 *   @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>.
 *   @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 *   @version $Revision: 1.17.2.6 $
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
   protected org.javagroups.JChannel channel;

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
      log.debug("Creating JavaGroups JChannel");

      this.channel = new org.javagroups.JChannel(jgProps);
      channel.setOpt(Channel.GET_STATE_EVENTS, new Boolean(true));	    
      channel.setOpt(Channel.AUTO_RECONNECT, new Boolean(true));
      channel.setOpt(Channel.AUTO_GETSTATE, new Boolean(true));

      // Force JavaGroups to use log4j
      Logger category = Logger.getLogger("org.javagroups."+partitionName);
      Level priority = category.getEffectiveLevel();
      /* Set the JavaGroups Trace level based on the log4j Level. The mapping
      is:
      Log4j Level    JavaGroups Trace
      TRACE          DEBUG
      DEBUG          ERROR
      INFO           ERROR
      ERROR          ERROR
      WARN           WARN
      FATAL          FATAL

      Neither the JavaGroups Trace.TEST level or Trace.INFO is used as its too
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
      log.info("Set the JavaGroups logging to log4j with category:"+category
         +", priority: "+priority+", JavaGroupsLevel: "+traceLevel);
      log.debug("Creating HAPartition");
      partition = new HAPartitionImpl(partitionName, channel, deadlock_detection, getServer());
      log.debug("Initing HAPartition: " + partition);
      partition.init();
      log.debug("HAPartition initialized");
   }

   protected void startService() 
      throws Exception
   {
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
}
