
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.jca.ejb;





import java.rmi.MarshalledObject;
import java.rmi.RemoteException;
import java.sql.Connection;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.sql.DataSource;
import org.apache.log4j.Category;

/**
 * ConnectionFactorySerializationTestSessionBean.java
 *
 *
 * Created: Thu May 23 23:30:27 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 *
 * @ejb:bean   name="ConnectionFactorySerializationTestSession"
 *             jndi-name="ConnectionFactorySerializationTestSession"
 *             view-type="remote"
 *             type="Stateless"
 *
 */

public class ConnectionFactorySerializationTestSessionBean 
   implements SessionBean  
{

   private final Category log = Category.getInstance(getClass());

   /**
    * Describe <code>testConnectionFactorySerialization</code> method here.
    *
    * @exception EJBException if an error occurs
    * @ejb:interface-method
    */
   public void testConnectionFactorySerialization() 
   {
      try 
      {
         DataSource ds = (DataSource)new InitialContext().lookup("java:/DefaultDS");
         Connection c = ds.getConnection();
         c.close();
         MarshalledObject mo = new MarshalledObject(ds);
         ds = (DataSource)mo.get();
         c = ds.getConnection();
         c.close();
      }
      catch (Exception e)
      {
         log.info("Exception: ", e);
         throw new EJBException("Exception: " + e);
      } // end of try-catch
   }

   public void ejbCreate() 
   {
   }

   public void ejbActivate() throws RemoteException
   {
   }

   public void ejbPassivate() throws RemoteException
   {
   }

   public void ejbRemove() throws RemoteException
   {
   }

   public void setSessionContext(SessionContext ctx) throws RemoteException
   {
   }

   public void unsetSessionContext() throws RemoteException
   {
   }

}

