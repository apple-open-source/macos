/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
 
package org.jboss.iiop;

import org.omg.CORBA.ORB;

/**
 * Singleton class to ensure that all code running in the JBoss VM uses the 
 * same ORB instance. The CorbaORBService MBean calls CorbaORB.setInstance()
 * at service creation time, after it creates an ORB instance. Code that runs
 * both in the server VM and in client VM calls CorbaORB.getInstance() to get 
 * an ORB.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1.2.1 $
 */
public class CorbaORB
{
   /** The ORB instance in this VM. */
   private static ORB instance;

   /** Enforce non-instantiability. */
   private CorbaORB()
   {
   }
   
   /** 
    * This method is called only by the CorbaORBService MBean, so it has
    * package visibility. 
    */
   static void setInstance(ORB orb)
   {
      if (instance == null)
	 instance = orb;
      else
	 throw new RuntimeException(CorbaORB.class.getName() 
				    + ".setInstance() called more than once");
   }

   /**
    * This method is called by classes that are used both at the server and at 
    * the client side: the handle impl (org.jboss.proxy.ejb.HandleImplIIOP),
    * the home handle impl (org.jboss.proxy.ejb.HomeHandleImplIIOP),
    * and the home factory (org.jboss.proxy.ejb.IIOPHomeFactory).
    * When called by code running in the same VM as the the JBoss server,
    * getInstance() returns the ORB instance used by the CorbaORBService MBean 
    * (which previously issued a setInstance() call). Otherwise getInstance() 
    * returns an ORB instance obtained with an ORB.init() call.
    */
   public static ORB getInstance() 
   {
      if (instance == null) 
	 instance = ORB.init(new String[0], System.getProperties());
      return instance;
   }

}
