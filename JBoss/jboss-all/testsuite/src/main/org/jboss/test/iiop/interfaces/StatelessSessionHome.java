/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.interfaces;

import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBHome;

/**
 *   @author reverbel@ime.usp.br
 *   @version $Revision: 1.1.2.1 $
 */
public interface StatelessSessionHome
   extends EJBHome
{
   public static final String COMP_NAME = "java:comp/env/ejb/IIOPSession";
   public static final String JNDI_NAME = "IIOPSession";
   
   public StatelessSession create() throws CreateException, RemoteException;
}
