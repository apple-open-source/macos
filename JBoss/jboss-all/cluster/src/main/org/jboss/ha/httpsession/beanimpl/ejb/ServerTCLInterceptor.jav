package org.jboss.ha.httpsession.beanimpl.ejb;

import org.jboss.ejb.plugins.AbstractInterceptor;
import org.jboss.invocation.Invocation;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ServerTCLInterceptor extends AbstractInterceptor
{
   public Object invokeHome(Invocation mi) throws Exception
   {
      ClassLoader prevTCL = Thread.currentThread().getContextClassLoader();
      ClassLoader tcl = (ClassLoader) mi.getValue("org.jboss.invocation.TCL");
      try
      {
         // Set the active TCL to that of the invocation TCL
         if( tcl != null )
            Thread.currentThread().setContextClassLoader(tcl);
         return getNext().invokeHome(mi);
      }
      finally
      {
         // Restore the incoming TCL if we overrode it
         if( tcl != null )
            Thread.currentThread().setContextClassLoader(prevTCL);
      }
   }

   public Object invoke(Invocation mi) throws Exception
   {
      ClassLoader prevTCL = Thread.currentThread().getContextClassLoader();
      ClassLoader tcl = (ClassLoader) mi.getValue("org.jboss.invocation.TCL");
      try
      {
         // Set the active TCL to that of the invocation TCL
         if( tcl != null )
            Thread.currentThread().setContextClassLoader(tcl);
         return getNext().invoke(mi);
      }
      finally
      {
         // Restore the incoming TCL if we overrode it
         if( tcl != null )
            Thread.currentThread().setContextClassLoader(prevTCL);
      }
   }
}
