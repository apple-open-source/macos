/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.excepiiop.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

public interface ExceptionThrowerHome
   extends EJBHome
{
   // Constants -----------------------------------------------------
   public static final String COMP_NAME = "java:comp/env/ejb/ExceptionThrower";
   public static final String JNDI_NAME = "excepiiop/ExceptionThrower";

   public ExceptionThrower create() throws CreateException, RemoteException;
   
}
