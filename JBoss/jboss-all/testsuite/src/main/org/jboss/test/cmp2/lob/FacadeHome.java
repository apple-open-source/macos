/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.cmp2.lob;

/**
 *
 * @author  <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 */
public interface FacadeHome
   extends javax.ejb.EJBHome
{
   public static final String COMP_NAME="java:comp/env/ejb/Facade";
   public static final String JNDI_NAME="Facade";

   public Facade create()
      throws javax.ejb.CreateException,java.rmi.RemoteException;
}
