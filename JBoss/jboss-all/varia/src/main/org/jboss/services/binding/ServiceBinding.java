/*
 *  JBoss, the OpenSource J2EE webOS
 *
 *  Distributable under LGPL license.
 *  See terms of license at gnu.org.
 */

package org.jboss.services.binding;

import java.net.InetAddress;
import java.net.UnknownHostException;

/** A ServiceBinding is a {name,virtualHost,port,interfaceAddress}
 * quad specifying a named binding for a service.
 *
 * @author <a href="mailto:bitpushr@rochester.rr.com">Mike Finn</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ServiceBinding implements Cloneable
{
   /** The name of the binding. A null or empty name implies the default
     binding for a service.
    */
   private String name = null;
   /** The virtual host name. This is the interface name used to
    construct the bindAddress value. A null value implies bind on any
    interface.
    */
   private String hostName = null;
   /** The port the service should listen on. A 0 value implies an
    anonymous port.
    */
   private int port = 0;
   /** The interface on which the service should bind its listening port. A
    null address implies bind on any interface.
    */
   private InetAddress bindAddress = null;

   /** Make a copy of the ServiceBinding
    */
   public Object clone()
   {
      Object copy = null;
      try
      {
         copy = super.clone();
      }
      catch(CloneNotSupportedException cantHappend)
      {
      }
      return copy;
   }

   /**
    * Creates a new instance of ServiceDescriptor
    *
    * @param name The name of the binding. A null or empty name
    * implies that default binding for a service.
    * @param hostName The virtual host name. This is the interface name used to
    * construct the bindAddress value. A null value implies bind on any
    * interface.
    * @param port The port the service should listen on. A 0 value implies an
    * anonymous port.
    *
    * @exception UnknownHostException If hostName is not resolvable.
    */
   public ServiceBinding(String name, String hostName, int port)
      throws UnknownHostException
   {
      this.setName(name);
      this.setHostName(hostName);
      this.setBindAddress(hostName);
      this.setPort(port);
   }

   /**
    * Getter for property name.
    *
    * @return The name of the binding
    */
   public String getName()
   {
      return this.name;
   }

   /**
    * Setter for property name.
    *
    * @param name the name of the binding
    */
   public void setName(String name)
   {
      this.name = name;
   }

   /**
    * Sets the bindAddress attribute of the ServiceDescriptor object
    *
    * @param  pInetAddr  The new bindAddress value
    */
   public void setBindAddress(InetAddress bindAddress)
   {
      this.bindAddress = bindAddress;
   }
   
   /**
    * Sets the bindAddress, given a hostname
    *
    * @param pHostName The hostname with which to create an InetAddress
    *
    * @exception UnknownHostException Hostname is not resolvable
    */
   public void setBindAddress(String hostName)
      throws UnknownHostException
   {
      this.bindAddress = InetAddress.getByName(hostName);
   }

   /**
    * Gets the bindAddress attribute of the ServiceDescriptor object
    *
    * @return  The listen address
    */
   public InetAddress getBindAddress()
   {
      return this.bindAddress;
   }

   /**
    * Sets the port attribute of the ServiceDescriptor object
    *
    * @param  pPort  The new listen port number
    */
   public void setPort(int port)
   {
      this.port = port;
   }

   /**
    * Gets the port attribute of the ServiceDescriptor object
    *
    * @return The listen port number
    */
   public int getPort()
   {
      return this.port;
   }

   /**
    * Returns host name
    *
    * @return the hostname or address
    */
   public String getHostName()
   {
      return this.hostName;
   }

   /**
    * Sets the host name
    *
    * @param hostName, the hostname or address
    */
   public void setHostName(String hostName)
   {
      this.hostName = hostName;
   }

   /**
    * Create string representation of the service descriptor
    *
    * @return  String containing service descriptor properties
    */
   public String toString()
   {
      StringBuffer sBuf = new StringBuffer("ServiceBinding [name=");
      String host = getHostName();

      if (hostName == null)
      {
         host = "<ANY>";
      }

      sBuf.append(this.getName());
      sBuf.append(";hostName=");
      sBuf.append(host);
      sBuf.append(";bindAddress=");
      sBuf.append(this.getBindAddress().toString());
      sBuf.append(";port=");
      sBuf.append(this.getPort());
      sBuf.append("]");
      return sBuf.toString();
   }
}
