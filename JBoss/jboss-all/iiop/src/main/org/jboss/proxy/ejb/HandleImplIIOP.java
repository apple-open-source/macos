/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.proxy.ejb;

import java.rmi.RemoteException;
import java.util.Properties;
import javax.ejb.EJBObject;
import javax.ejb.Handle;
import javax.rmi.PortableRemoteObject;

import org.jboss.iiop.CorbaORB;

/**
 * A CORBA-based EJBObject handle implementation.
 *      
 * @author  <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>.
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1.2.1 $
 */
public class HandleImplIIOP
    implements Handle
{

   /**
    * This handle encapsulates an stringfied CORBA reference for an 
    * <code>EJBObject</code>. 
    */
   private String ior;
   
   /**
    * Constructs an <code>HandleImplIIOP</code>.
    *
    * @param ior An stringfied CORBA reference for an <code>EJBObject</code>.
    */
   public HandleImplIIOP(String ior) 
   {
      this.ior = ior;
   }
   
   /**
    * Constructs an <tt>HandleImplIIOP</tt>.
    *
    * @param obj An <code>EJBObject</code>.
    */
   public HandleImplIIOP(EJBObject obj) 
   {
      this((org.omg.CORBA.Object)obj);
   }
   
   /**
    * Constructs an <tt>HandleImplIIOP</tt>.
    *
    * @param obj A CORBA reference for an <code>EJBObject</code>.
    */
   public HandleImplIIOP(org.omg.CORBA.Object obj) 
   {
      this.ior = CorbaORB.getInstance().object_to_string(obj);
   }
   
   // Public --------------------------------------------------------
   
   // Handle implementation -----------------------------------------
   
   /**
    * Obtains the <code>EJBObject</code> represented by this handle.
    *
    * @return  a reference to an <code>EJBObject</code>.
    *
    * @throws RemoteException
    */
   public EJBObject getEJBObject() 
         throws RemoteException 
   {
      try 
      {
         return (EJBObject)PortableRemoteObject.narrow(
                                  CorbaORB.getInstance().string_to_object(ior),
                                  EJBObject.class);
      }
      catch (Exception e) 
      {
         throw new RemoteException("Could not get EJBObject from Handle");
      }
   }

}
