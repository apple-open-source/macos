/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.id;

import java.net.InetAddress;

import java.security.AccessController;
import java.security.PrivilegedAction;

import org.jboss.util.Primitives;
import org.jboss.util.HashCode;
import org.jboss.util.platform.PID;

/**
 * An object that uniquely identifies a virtual machine.
 *
 * <p>The identifier is composed of:
 * <ol>
 *    <li>The Internet address of the physical machine.</li>
 *    <li>The process identifier of the virtual machine.</li>
 *    <li>A UID to guarantee uniqness across multipule virtual
 *        machines on the same physical machine.</li>
 * </ol>
 *
 * <pre>
 *    [ address ] - [ process id ] - [ time ] - [ counter ]
 *                                   |------- UID --------|
 * </pre>
 *
 * <p>Numbers are converted to radix(Character.MAX_RADIX) when converting
 *    to strings.
 *
 * @see UID
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class VMID
   implements ID
{
   /** The address of the current virtual machine */
   protected final byte[] address;

   /** The process identifier of the current virtual machine */
   protected final PID pid;

   /** A unique identifier to ensure uniqueness across the host machine */
   protected final UID uid;

   /** The hash code of this VMID */
   protected final int hashCode;

   /**
    * Construct a new VMID.
    *
    * @param address    The address of the current virtual machine.
    * @param pid        Process identifier.
    * @param uid        Unique identifier.
    *
    * @see #getInstance()  For getting a VMID instance reference.
    */
   protected VMID(final byte[] address, final PID pid, final UID uid) {
      this.address = address;
      this.pid = pid;
      this.uid = uid;

      // generate a hashCode for this VMID
      int code = pid.hashCode();
      code ^= uid.hashCode();
      code ^= HashCode.generate(address);
      hashCode = code;
   }

   /**
    * Copy a VMID.
    *
    * @param vmid    VMID to copy.
    */
   protected VMID(final VMID vmid) {
      this.address = vmid.address;
      this.pid = vmid.pid;
      this.uid = vmid.uid;
      this.hashCode = vmid.hashCode;
   }

   /**
    * Get the address portion of this VMID.
    *
    * @return  The address portion of this VMID.
    */
   public final byte[] getAddress() {
      return address;
   }

   /**
    * Get the process identifier portion of this VMID.
    *
    * @return  The process identifier portion of this VMID.
    */
   public final PID getProcessID() {
      return pid;
   }

   /**
    * Get the UID portion of this VMID.
    *
    * @return  The UID portion of this VMID.
    */
   public final UID getUID() {
      return uid;
   }

   /**
    * Return a string representation of this VMID.
    *
    * @return  A string representation of this VMID.
    */
   public String toString() {
      StringBuffer buff = new StringBuffer();
      
      for (int i=0; i<address.length; i++) {
         int n = (int) (address[i] & 0xFF);
         buff.append(Integer.toString(n, Character.MAX_RADIX));
      }

      buff.append("-").append(pid.toString(Character.MAX_RADIX));
      buff.append("-").append(uid);

      return buff.toString();
   }

   /**
    * Return the hash code of this VMID.
    *
    * @return  The hash code of this VMID.
    */
   public final int hashCode() {
      return hashCode;
   }

   /**
    * Check if the given object is equal to this VMID.
    *
    * <p>A VMID is equals to another VMID if the address,
    *    process identifer and UID portions are equal.
    *
    * @param obj     Object to test equality with.
    * @return        True if object is equals to this VMID.
    */
   public boolean equals(final Object obj) {
      if (obj == this) return true;

      if (obj != null && obj.getClass() == getClass()) {
         VMID vmid = (VMID)obj;
         return 
            Primitives.equals(vmid.address, address) &&
            vmid.pid.equals(pid) &&
            vmid.uid.equals(uid);
      }

      return false;
   }

   /**
    * Returns a copy of this VMID.
    *
    * @return  A copy of this VMID.
    */
   public Object clone() {
      try {
         return super.clone();
      }
      catch (CloneNotSupportedException e) {
         throw new InternalError();
      }
   }

   /**
    * Returns a VMID as a string.
    *
    * @return  VMID as a string.
    */
   public static String asString() {
      return getInstance().toString();
   }


   /////////////////////////////////////////////////////////////////////////
   //                            Instance Access                          //
   /////////////////////////////////////////////////////////////////////////

   /** The single instance of VMID for the running Virtual Machine */
   private static VMID instance = null;

   /**
    * Get the VMID for the current virtual machine.
    *
    * @return  Virtual machine identifier.
    *
    * @throws NestedError  Failed to create VMID instance.
    */
   public synchronized static VMID getInstance() {
      if (instance == null) {
         instance = create();
      }
      return instance;
   }

   /** 
    * The address used when conventional methods fail to return the address
    * of the current machine.
    */
   public static final byte[] UNKNOWN_HOST = { 0, 0, 0, 0 };

   /**
    * Return the current host internet address.
    */
   private static byte[] getHostAddress() {
      return (byte[]) AccessController.doPrivileged(new PrivilegedAction() {
            public Object run() {
               try {
                  return InetAddress.getLocalHost().getAddress();
               }
               catch (Exception e) {
                  return UNKNOWN_HOST;
               }
            }
         });
   }

   /**
    * Create the VMID for the current virtual mahcine.
    *
    * @return  Virtual machine identifer.
    */
   private static VMID create() {
      // get the local internet address for the current host
      byte[] address = getHostAddress();
         
      return new VMID(address, PID.getInstance(), new UID());
   }
}
