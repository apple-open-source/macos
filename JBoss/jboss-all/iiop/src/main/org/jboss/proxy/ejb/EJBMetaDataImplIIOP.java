/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.proxy.ejb;

import java.io.Serializable;
import java.rmi.RemoteException;

import javax.ejb.HomeHandle;
import javax.ejb.EJBMetaData;
import javax.ejb.EJBHome;
import javax.ejb.EJBException;

/**
 * An implementation of the EJBMetaData interface which allows a
 * client to obtain the enterprise Bean's meta-data information.
 *      
 * @author  Rickard Öberg (rickard.oberg@telkel.com)
 * @author  <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:reverbel@ime.usp.br">Francisco Reverbel</a>
 * @version $Revision: 1.1 $
 */
public class EJBMetaDataImplIIOP
      implements EJBMetaData, Serializable 
{
   
   // Attributes ----------------------------------------------------
   
   private final Class remoteClass;
   private final Class homeClass;
   private final Class pkClass;
   private final boolean session;
   private final boolean statelessSession;
   private final EJBHome home;
        
   // Constructors --------------------------------------------------
   
   /**
    * Constructs an <code>EJBMetaDataImplIIOP</code>.
    */
   public EJBMetaDataImplIIOP(Class remoteClass,
                              Class homeClass,
                              Class pkClass,
                              boolean session,
                              boolean statelessSession,
                              EJBHome home) 
   {
      this.remoteClass = remoteClass;
      this.homeClass = homeClass;
      this.pkClass = pkClass;
      this.session = session;
      this.statelessSession = statelessSession;
      this.home = home;
   }
        
   // EJBMetaData ---------------------------------------------------

   /**
    * Obtains the home interface of the enterprise Bean.
    */
   public EJBHome getEJBHome() { return home; }

   /**
    * Obtains the <code>Class</code> object for the enterprise Bean's home 
    * interface.
    */
   public Class getHomeInterfaceClass() { return homeClass; }
   
   /**
    * Obtains the <code>Class</code> object for the enterprise Bean's remote 
    * interface.
    */
   public Class getRemoteInterfaceClass() { return remoteClass; }
   
   /**
    * Obtains the <code>Class</code> object for the enterprise Bean's primary 
    * key class. 
    */
   public Class getPrimaryKeyClass() { return pkClass; }
   
   /**
    * Tests if the enterprise Bean's type is "session".
    *
    * @return true if the type of the enterprise Bean is session bean.
    */    
   public boolean isSession() { return session; }
   
   /**
    * Tests if the enterprise Bean's type is "stateless session".
    *
    * @return true if the type of the enterprise Bean is stateless session.
    */
   public boolean isStatelessSession() { return statelessSession; }
}
