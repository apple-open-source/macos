/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import java.net.InetAddress;
import org.jgroups.stack.IpAddress;

/**
 * Replacement for a JG IpAddress that doesn't base its representation
 * on the JG address but on the computed node name added to the IPAddress instead.
 * This is to avoid any problem in the cluster as some nodes may interpret a node name
 * differently (IP resolution, name case, FQDN or host name, etc.)
 *
 * @see org.jboss.ha.framework.server.ClusterPartition
 *
 * @author  <a href="mailto:sacha.labourey@jboss.org">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.4 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>August 17 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public class ClusterNode
   implements Comparable, Cloneable
{

   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------

   protected String id = null;
   protected String jgId = null;
   protected IpAddress originalJGAddress = null;

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
       
   public ClusterNode()
   {
   }

   public ClusterNode(IpAddress jgAddress)
   {
      if (jgAddress.getAdditionalData() == null)
      {
         this.id = jgAddress.getIpAddress().getHostAddress() + ":" + jgAddress.getPort();
      }
      else
      {
         this.id = new String(jgAddress.getAdditionalData());
      }

      this.originalJGAddress = jgAddress;
      StringBuffer sb = new StringBuffer();
      java.net.InetAddress jgIPAddr = jgAddress.getIpAddress();
      if (jgIPAddr == null)
         sb.append("<null>");
      else
      {
         if (jgIPAddr.isMulticastAddress())
            sb.append(jgIPAddr.getHostAddress());
         else
            sb.append(getShortName(jgIPAddr.getHostName()));
      }
      sb.append(":" + jgAddress.getPort());
      this.jgId = sb.toString();
   }

   // Public --------------------------------------------------------

   public String getName()
   {
      return this.id;
   }

   public String getJGName()
   {
      return this.jgId;
   }

   public IpAddress getOriginalJGAddress()
   {
      return this.originalJGAddress;
   }
   public InetAddress getIpAddress()
   {
      return this.originalJGAddress.getIpAddress();
   }
   public int getPort()
   {
      return this.originalJGAddress.getPort();      
   }

   // Comparable implementation ----------------------------------------------

   // Comparable implementation ----------------------------------------------

   public int compareTo(Object o)
   {
      if ((o == null) || !(o instanceof ClusterNode))
         throw new ClassCastException("ClusterNode.compareTo(): comparison between different classes");

      ClusterNode other = (ClusterNode) o;

      return this.id.compareTo(other.id);
   }
   // java.lang.Object overrides ---------------------------------------------------

   public boolean equals(Object obj)
   {
      if (obj == null) return false;
      return compareTo(obj) == 0 ? true : false;
   }

   public int hashCode()
   {
      return id.hashCode();
   }

   public String toString()
   {
      return this.getName();
   }

   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------

   protected String getShortName(String hostname)
   {
      int index = hostname.indexOf('.');

      if (hostname == null) return "";
      if (index > 0 && !Character.isDigit(hostname.charAt(0)))
         return hostname.substring(0, index);
      else
         return hostname;
   }

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

}
