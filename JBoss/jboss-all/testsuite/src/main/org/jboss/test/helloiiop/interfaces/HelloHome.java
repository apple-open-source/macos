/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.helloiiop.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.1 $
 */
public interface HelloHome
   extends EJBHome
{
   // Constants -----------------------------------------------------
   public static final String COMP_NAME = "java:comp/env/ejb/Hello";
   public static final String JNDI_NAME = "helloworld/Hello";

   public Hello create() throws CreateException, RemoteException;
   
}
