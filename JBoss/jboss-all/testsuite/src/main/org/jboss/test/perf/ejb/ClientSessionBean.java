package org.jboss.test.perf.ejb;

import java.rmi.RemoteException;
import java.rmi.ServerException;
import javax.ejb.CreateException;
import javax.ejb.RemoveException;
import javax.ejb.SessionContext;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import org.apache.log4j.Logger;

import org.jboss.test.perf.interfaces.SessionHome;
import org.jboss.test.perf.interfaces.Session;

/** An implementation of the Session interface that delegates its calls
 to the SessionBean implementation to test session to session bean timings.

@version $Revision: 1.6 $
*/
public class ClientSessionBean implements javax.ejb.SessionBean
{
   private static Logger log = Logger.getLogger(ClientSessionBean.class);
   private String entityName;

   public void create(int low, int high)
      throws CreateException, RemoteException
   {
      try
      {
         long start = System.currentTimeMillis();
         Session bean = lookupSession();
         bean.create(low, high);
         long end = System.currentTimeMillis();
         log.debug("create ran in: "+(end - start));
      }
      catch (CreateException ce)
      {
         throw ce;
      }
      catch(Exception e)
      {
         log.error("create failed", e);
         throw new CreateException(e.toString());
      }
   }

   public void remove(int low, int high)
      throws RemoveException, RemoteException
   {
      try
      {
         long start = System.currentTimeMillis();
         Session bean = lookupSession();
         bean.remove(low, high);
         long end = System.currentTimeMillis();
         log.debug("remove ran in: "+(end - start));
      }
      catch(Exception e)
      {
         throw new RemoteException("remove failure", e);
      }
   }

   public void read(int id) throws RemoteException
   {
      try
      {
         long start = System.currentTimeMillis();
         Session bean = lookupSession();
         bean.read(id);
         long end = System.currentTimeMillis();
         log.debug("read ran in: "+(end - start));
      }
      catch(Exception e)
      {
         throw new RemoteException("read failure", e);
      }
   }

   public void read(int low, int high) throws RemoteException
   {
      try
      {
         long start = System.currentTimeMillis();
         Session bean = lookupSession();
         bean.read(low, high);
         long end = System.currentTimeMillis();
         log.debug("read ran in: "+(end - start));
      }
      catch(Exception e)
      {
         throw new RemoteException("read failure", e);
      }
   }

   public void write(int id) throws RemoteException
   {
      try
      {
         long start = System.currentTimeMillis();
         Session bean = lookupSession();
         bean.write(id);
         long end = System.currentTimeMillis();
         log.debug("write ran in: "+(end - start));
      }
      catch(Exception e)
      {
         throw new RemoteException("write failure", e);
      }
   }

   public void write(int low, int high) throws RemoteException
   {
      try
      {
         long start = System.currentTimeMillis();
         Session bean = lookupSession();
         bean.write(low, high);
         long end = System.currentTimeMillis();
         log.debug("write ran in: "+(end - start));
      }
      catch(Exception e)
      {
         throw new RemoteException("write failure", e);
      }
   }

   public void setSessionContext(SessionContext context)
   {
   }
   public void ejbCreate(String entityName) throws CreateException
   {
      this.entityName = entityName;
   }
   public void ejbRemove()
   {
   }
   public void ejbActivate()
   {
   }
   public void ejbPassivate()
   {
   }

   private Session lookupSession() throws Exception
   {
      Context context = new InitialContext();
      Object ref = context.lookup("java:comp/env/ejb/Session");
      SessionHome home = (SessionHome) PortableRemoteObject.narrow(ref, SessionHome.class);
      Session bean = home.create(entityName);
      return bean;
   }

}
