/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces.impl;

import org.jboss.console.manager.interfaces.ManageableResource;

import javax.management.ObjectName;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>December 16, 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class MBeanResource
   implements ManageableResource
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   String className = null;
   ObjectName oj = null;
   transient Object mbean = null; // SUPPORT FOR REMOTE MBEANS!!!!
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public MBeanResource () {}
   
   public MBeanResource (ObjectName oj, String clazz)
   {
      this.oj = oj;
      this.className = clazz;
   }
   
   public MBeanResource (ObjectName oj, String clazz, Object proxy)
   {
      this.oj = oj;
      this.className = clazz;
      this.mbean = proxy;
   }
   
   // Public --------------------------------------------------------
   
   public String getClassName ()
   {
      return this.className;
   }
   
   public ObjectName getObjectName ()
   {
      return this.oj;
   }
   
   public Object getMBeanProxy ()
   {
      return this.mbean;
   }   
   
   // ManageableResource implementation ----------------------------------------------
   
   public String getId ()
   {
      return this.oj.toString ();
   }
   
   // Object overrides ---------------------------------------------------
   
   public boolean equals (Object other)
   {
      if (other instanceof MBeanResource)
         return this.oj.equals (((MBeanResource)other).oj);
      else
         return false;
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
