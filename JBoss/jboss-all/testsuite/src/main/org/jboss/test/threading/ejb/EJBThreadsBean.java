/**
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*
*/
package org.jboss.test.threading.ejb;

import java.rmi.*;
import javax.ejb.*;

/**
*   <description> 
*
*   @see <related>
*   @author  <a href="mailto:marc@jboss.org">Marc Fleury</a>
*   @version $Revision: 1.4 $
*   
*   Revisions:
*
*   20010625 marc fleury: Initial version
*/
public class EJBThreadsBean implements EntityBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
   // Constants -----------------------------------------------------
  
   // Attributes ----------------------------------------------------
   public String id;
   // Static --------------------------------------------------------
  
  // Constructors --------------------------------------------------
  
  // Public --------------------------------------------------------
 
	
   public String ejbCreate(String id) 
      throws RemoteException, CreateException {
      log.debug("create"+Thread.currentThread()+" id: "+id);
      this.id = id;
		
      return id;
   }
	
   public void ejbPostCreate(String id)  {}
   public void ejbRemove() throws RemoveException {log.debug("remove"+Thread.currentThread());}
   public void ejbActivate() throws RemoteException {}
   public void ejbPassivate() throws RemoteException {}
   public void ejbLoad() throws RemoteException {}
   public void ejbStore() throws RemoteException {}
	
   public void setEntityContext(EntityContext context) throws RemoteException {}
   public void unsetEntityContext() throws RemoteException{}
	
   public void test() {
		
      log.debug("test"+Thread.currentThread());
			
   }
	
   public void testBusinessException() 
      throws RemoteException
   {
      log.debug("testBusinessExcetiopn"+Thread.currentThread());
      throw new RemoteException("TestBusinessException");
   };
	
   public void testRuntimeException() 
      throws RemoteException
   {
      log.debug("testRuntimeException"+Thread.currentThread());
      throw new NullPointerException();
   }
   public void testTimeOut()
      throws RemoteException
   {
      log.debug("testTimeout"+Thread.currentThread());
      synchronized (this)
      {
         try {
            wait(5000);
         }
         catch (InterruptedException e) {
				
         }
      }
   }
   public void testNonTransactional() throws RemoteException
   {
      log.debug("testNonTransactional"+Thread.currentThread());
   }
}
