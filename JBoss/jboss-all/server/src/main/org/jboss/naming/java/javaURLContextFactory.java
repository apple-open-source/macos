/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.naming.java;

import java.util.Hashtable;
import javax.naming.*;
import javax.naming.spi.*;

import org.jnp.interfaces.NamingContext;
import org.jnp.server.NamingServer;

/**
 *   Implementation of "java:" namespace factory. java: is a VM-local namespace.
 *     
 *   @see <related>
 *   @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 *   @version $Revision: 1.8 $
 */
public class javaURLContextFactory
   implements ObjectFactory
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
    
   // Static --------------------------------------------------------
   static NamingServer root;
   
   static
   {
      try
      {
         root = new NamingServer();
      } catch (NamingException e)
      {
         e.printStackTrace();
      }
   }
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------

   // ObjectFactory implementation ----------------------------------
   public Object getObjectInstance(Object obj,
                                Name name,
                                Context nameCtx,
                                Hashtable environment)
                         throws Exception
   {
      return new NamingContext(environment, name, root);
   }
}
