/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management;

import java.security.BasicPermission;

/**
 * Controls access to actions performed on MBeanServers. The name specifies
 * the permission applies to an operation. The special value * applies to
 * all operations.
 *
 * <ul>
 * <li><b>createMBeanServer<b> controls access to 
 *     {@link MBeanServerFactory#createMBeanServer()} or 
 *     {@link MBeanServerFactory#createMBeanServer(java.lang.String)} </li>
 * <li><b>findMBeanServer<b> controls access to 
 *     {@link MBeanServerFactory#findMBeanServer(java.lang.String)} </li>
 * <li><b>newMBeanServer<b> controls access to 
 *     {@link MBeanServerFactory#newMBeanServer()} or 
 *     {@link MBeanServerFactory#newMBeanServer(java.lang.String)} </li>
 * <li><b>releaseMBeanServer<b> controls access to 
 *     {@link MBeanServerFactory#releaseMBeanServer(javax.management.MBeanServer)} </li>
 * 
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1.2.1 $
 */
public class MBeanServerPermission
   extends BasicPermission
{
   // Constants ---------------------------------------------------

   private static final long serialVersionUID = -5661980843569388590L;

   // Attributes --------------------------------------------------

   // Static ------------------------------------------------------

   // Constructors ------------------------------------------------

   /**
    * Construct a new MBeanServer permission for a given name
    *
    * @param name the name of the permission to grant
    * @exception NullPointerExecption if the name is null
    * @exception IllegalArgumentException if the name is not * or one of
    *            listed names
    */
   public MBeanServerPermission(String name)
   {
      this(name, null);
   }

   /**
    * Construct a new MBeanServer permission for a given name
    *
    * @param name the name of the permission to grant
    * @param actions unused
    * @exception NullPointerExecption if the name is null
    * @exception IllegalArgumentException if the name is not * or one of
    *            listed names
    */
   public MBeanServerPermission(String name, String actions)
   {
      super(name, actions);
      if (name.equals("*") == false &&
          name.equals("createMBeanServer") == false &&
          name.equals("findMBeanServer") == false &&
          name.equals("newMBeanServer") == false &&
          name.equals("releaseMBeanServer") == false)
         throw new IllegalArgumentException("Unknown name: " + name);
   }

   // Public ------------------------------------------------------

   /**
    * @return human readable string.
    */
   public String toString()
   {
      StringBuffer buffer = new StringBuffer(100);
      buffer.append(getClass().getName()).append(":");
      buffer.append(" name=").append(getName());
      buffer.append(" actions=").append(getActions());
      return buffer.toString();
   }

   // X Implementation --------------------------------------------

   // Y Overrides -------------------------------------------------

   // Protected ---------------------------------------------------

   // Private -----------------------------------------------------

   // Inner classes -----------------------------------------------
}
