/*
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jnp.interfaces;

import java.rmi.Remote;
import java.rmi.RemoteException;
import java.util.Collection;


import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NamingException;

/**
 *   <description> 
 *      
 *   @see <related>
 *   @author $Author: oberg $
 *   @version $Revision: 1.3 $
 */
public interface Naming
   extends Remote
{
   // Public --------------------------------------------------------
   public void bind(Name name, Object obj, String className)
      throws NamingException, RemoteException;

   public void rebind(Name name, Object obj, String className)
      throws NamingException, RemoteException;

   public void unbind(Name name)
      throws NamingException, RemoteException;

   public Object lookup(Name name)
      throws NamingException, RemoteException;

   public Collection list(Name name)
      throws NamingException, RemoteException;

   public Collection listBindings(Name name)
      throws NamingException, RemoteException;
      
   public Context createSubcontext(Name name)
      throws NamingException, RemoteException;
}
