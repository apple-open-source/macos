/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.proxy.ejb;

import java.util.Hashtable;
import javax.ejb.EJBHome;
import javax.naming.Context;
import javax.naming.Name;
import javax.naming.Reference;
import javax.naming.spi.ObjectFactory;
import javax.rmi.PortableRemoteObject;

import org.jboss.iiop.CorbaORB;

/**
 * An <code>ObjectFactory</code> implementation that translates
 * <code>Reference</code>s to <code>EJBHome</code>s back into CORBA
 * object references. The IIOP proxy factory (IORFactory) binds these 
 * <code>Reference</code>s in the JRMP/JNDI namespace.
 *
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1.2.1 $
 */
public class IIOPHomeFactory implements ObjectFactory 
{

   public IIOPHomeFactory()
   {
   }
   
   // Implementation of the interface ObjectFactory ------------------------
   
   public Object getObjectInstance(Object obj, Name name,
                                   Context nameCtx, Hashtable environment)
      throws Exception
   {
      return (EJBHome)PortableRemoteObject.narrow(
                  CorbaORB.getInstance().string_to_object(
                        (String)((Reference)obj).get("IOR").getContent()),
                  EJBHome.class);
   }
}
