/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: EJBHomeProvider.java,v 1.1 2002/04/02 13:48:39 cgjung Exp $

package org.jboss.net.axis.server;

import org.apache.axis.Handler;
import org.apache.axis.AxisFault;
import org.apache.axis.MessageContext;
import org.apache.axis.providers.java.RPCProvider;

import javax.naming.InitialContext;
import javax.naming.Context;

import javax.ejb.EJBHome;

import java.lang.reflect.Method;

/**
 * A JBoss-compatible Provider that exposes the methods of
 * a bean´s home, such as a stateless session bean or an entity
 * bean. It is working under the presumption that the right classloader 
 * has already been set by the invocation chain 
 * (@see org.jboss.net.axis.SetClassLoaderHandler).
 * <br>
 * <h3>Change History</h3>
 * <ul>
 * </ul>
 * <br>
 * <h3>To Do</h3>
 * <ul>
 * <li> jung, 22.03.02: Service-Reference serialisation. </li>
 * </ul>
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 22.03.2002
 * @version $Revision: 1.1 $
 */

public class EJBHomeProvider extends RPCProvider {

   private static final String beanNameOption = "beanJndiName";
   private static final String homeInterfaceNameOption = "homeInterfaceName";

   /** Creates new EJBProvider */
   public EJBHomeProvider() {
   }

   /**
    * Return the object which implements the service. Makes the usual
    * JNDI->lookup call wo the  PortableRemoteDaDaDa for the sake of Corba.
    * @param msgContext the message context
    * @param clsName The JNDI name of the EJB home class
    * @return an object that implements the service
    */
   protected Object getNewServiceObject(MessageContext msgContext, String clsName)
      throws Exception {
      // Get the EJB Home object from JNDI
      Object result = new InitialContext().lookup(clsName);

      return result;
   }
   
    /**
     * Return the option in the configuration that contains the service class
     * name.  In the EJB case, it is the JNDI name of the bean.
     */
    protected String getServiceClassNameOptionName()
    {
        return beanNameOption;
    }

    /**
     * Get the class description for the EJB Remote Interface, which is what
     * we are interested in exposing to the world (i.e. in WSDL).
     * 
     * @param msgContext the message context
     * @param beanJndiName the JNDI name of the EJB
     * @return the class info of the EJB home interface
     */ 
    protected Class getServiceClass(MessageContext msgContext, 
                                    String beanJndiName) throws Exception 
    {
        Handler serviceHandler = msgContext.getService();
        Class interfaceClass = null;
        
        // First try to get the interface class from the configuation
        String homeName = 
                (String) serviceHandler.getOption(homeInterfaceNameOption);
       if(homeName != null){
            interfaceClass = msgContext.getClassLoader().loadClass(homeName);
        } else {
           // we look into the metadata
           EJBHome home=(EJBHome) getNewServiceObject(msgContext,beanJndiName);
           interfaceClass=home.getEJBMetaData().getHomeInterfaceClass();
       }
            
        // got it, return it
       return interfaceClass;
    }

}