/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;


import java.util.Hashtable;
import org.jboss.invocation.MarshalledInvocation;

/** 
 *   When using HA-RMI, the RMI communication end-point on the server-side is
 *   an instance of this class. All invocations are sent through this servant
 *   that will route the call to the appropriate object and call the appropriate
 *   Java method.
 *
 *   @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 *   @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 *   @version $Revision: 1.6 $
 *
 * <p><b>Revisions:</b><br>
 */

public interface HARMIServer extends java.rmi.Remote
{
   public static Hashtable rmiServers = new Hashtable();

   /**
    * Performs an invocation through this HA-RMI for the target object hidden behind it.
    */   
   public HARMIResponse invoke (long tag, MarshalledInvocation mi) throws Exception;
   
   /**
    * Returns a list of node stubs that are current replica of this service.
    */   
   public java.util.List getReplicants () throws Exception;
   
   /**
    * Get local stub for this service.
    */   
   public Object getLocal() throws Exception;
}
