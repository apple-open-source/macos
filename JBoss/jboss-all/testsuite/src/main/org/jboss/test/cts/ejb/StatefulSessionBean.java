package org.jboss.test.cts.ejb;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.rmi.RemoteException;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.EJBObject;
import javax.ejb.Handle;
import javax.ejb.RemoveException;
import javax.ejb.SessionSynchronization;
import javax.naming.Context;
import javax.naming.InitialContext;

import org.apache.log4j.Category;
import org.jboss.test.cts.interfaces.BeanContextInfo;
import org.jboss.test.cts.interfaces.CtsCmpLocal;
import org.jboss.test.cts.interfaces.CtsCmpLocalHome;
import org.jboss.test.cts.interfaces.StatefulSession;
import org.jboss.test.cts.interfaces.StatefulSessionHome;
import org.jboss.test.cts.interfaces.StatelessSession;
import org.jboss.test.cts.interfaces.StatelessSessionHome;
import org.jboss.test.cts.keys.AccountPK;
import org.jboss.test.util.ejb.SessionSupport;


/** The stateful session ejb implementation
 *
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.17.2.4 $
 */
public class StatefulSessionBean
   extends SessionSupport
   implements SessionSynchronization
{
   private static transient Category log = Category.getInstance(StatefulSessionBean.class);
   private transient int counterAtTxStart;
   private String testName;
   private int counter;
   private CtsCmpLocal entityBean;
   private Context enc;
   private Handle sessionHandle;
   private SessionRef sessionRef;
   private byte[] statefulHandle;
   private boolean wasActivated;
   private boolean wasPassivated;

   public void ejbCreate(String testName)
   {
      this.testName = testName;
      log = Category.getInstance(StatefulSessionBean.class.getName()+"#"+testName);
      log.debug("ejbCreate("+testName+"), ctx="+sessionCtx);
   }
   public void ejbCreateAlt(String testName)
   {
      this.testName = testName + "Alt";
      log = Category.getInstance(StatefulSessionBean.class.getName()+"#"+testName);
      log.debug("ejbCreateAlt("+testName+"), ctx="+sessionCtx);
   }

   public void ejbActivate()
   {
      log = Category.getInstance(StatefulSessionBean.class.getName()+"#"+testName);
      log.debug("ejbActivate( )...");
      wasActivated = true;
   }
   public void ejbPassivate()
   {
      log.debug("ejbPassivate( )...");
      wasPassivated = true;
   }

   public void afterBegin ()
   {
      log.debug("afterBegin()..., counter="+counter);
      counterAtTxStart = counter;
   }
   public void afterCompletion (boolean isCommited)
   {
      log.debug("afterCompletion(), isCommited="+isCommited
         +", counter="+counter+", counterAtTxStart="+counterAtTxStart);
      if( isCommited == false )
      {
         counter = counterAtTxStart;
         log.debug("Rolling counter back to: "+counter);
      }
      else
      {
         log.debug("Committed updated counter: "+counter);         
      }
   }
   public void beforeCompletion ()
   {
      log.debug("beforeCompletion(), counter="+counter
         +", counterAtTxStart="+counterAtTxStart);
   }

   public String getTestName()
   {
      return testName;
   }

   public String method1(String msg)
   {
      log.debug("method1( ), msg="+msg);
      return msg;
   }

   public void incCounter ()
   {
      counter++;
   }

   public void decCounter ()
   {
      counter--;
   }

   public int getCounter ()
   {
      return counter;
   }

   public void setCounter (int value)
   {
      counter = value;
   }

   public BeanContextInfo getBeanContextInfo ()
      throws java.rmi.RemoteException
   {
      BeanContextInfo ctx = new BeanContextInfo();

      log.debug("Getting EJBObject..");
      Class remoteInterface = sessionCtx.getEJBObject().getClass();
      ctx.remoteInterface = remoteInterface.getName();

      log.debug("Getting EJBHome...");
      Class homeInterface = sessionCtx.getEJBHome().getClass();
      ctx.homeInterface = homeInterface.getName();

      log.debug("calling setRollbackOnly( ) on context");
      sessionCtx.setRollbackOnly();
      ctx.isRollbackOnly = new Boolean(sessionCtx.getRollbackOnly());

      return ctx;
   }

   public void loopbackTest ()
      throws java.rmi.RemoteException
   {
      try
      {
        Context ctx = new InitialContext();
        StatefulSessionHome home = (StatefulSessionHome) ctx.lookup("ejbcts/StatefulSessionBean");
        StatefulSession sessionBean;
        try 
        {
           sessionBean = home.create(testName);
        } 
        catch (CreateException crex) 
        {
           log.debug("Loopback CreateException: " + crex);
           throw new EJBException(crex);
        }
        sessionBean.loopbackTest(sessionCtx.getEJBObject());

      }
      catch (javax.naming.NamingException nex)
      {
         log.debug("Could not locate bean instance");
         throw new EJBException(nex);
      }
   }

   public void loopbackTest (EJBObject obj)
      throws java.rmi.RemoteException
   {
      // This should throw an exception. 
      StatefulSession bean = ( StatefulSession ) obj;

      bean.method1("Hello");
   }

   public void ping()
   {
   }

   public void sleep(long wait)
   {
      try
      {
         Thread.sleep(wait);
      }
      catch (InterruptedException e)
      {
         throw new EJBException("Interrupted", e);
      }
   }

   public boolean getWasActivated()
   {
      log.debug("getWasActivated( ), wasActivated="+wasActivated);
      return wasActivated;
   }

   public boolean getWasPassivated()
   {
      log.debug("getWasPassivated( ), wasPassivated="+wasPassivated);
      return wasPassivated;
   }

   public void createLocalEntity(AccountPK pk, String personsName)
         throws CreateException
   {
      try
      {
         InitialContext ctx = new InitialContext();
         enc = (Context) ctx.lookup("java:comp/env");
         CtsCmpLocalHome home = (CtsCmpLocalHome) enc.lookup("ejb/CMPBeanLocalHome");
         entityBean = home.create(pk, personsName);
      }
      catch(Exception e)
      {
         log.error("CtsCmpLocal create failed", e);
         throw new EJBException("CtsCmpLocal create failed", e);
      }
   }

   public String readAndRemoveEntity()
      throws RemoveException
   {
      String name = entityBean.getPersonsName();
      entityBean.remove();
      return name;
   }

   public void createSessionHandle()
   {
      log.info("createSessionHandle");
      try
      {
         InitialContext ctx = new InitialContext();
         enc = (Context) ctx.lookup("java:comp/env");
         StatelessSessionHome home = (StatelessSessionHome) enc.lookup("ejb/StatelessSessionHome");
         StatelessSession bean = home.create();
         sessionHandle = bean.getHandle();
      }
      catch(Exception e)
      {
         log.error("StatelessSessionHome create failed", e);
         throw new EJBException("StatelessSessionHome create failed", e);
      }
   }

   public String useSessionHandle(String arg)
   {
      log.info("useSessionHandle");
      try
      {
         StatelessSession bean = (StatelessSession) sessionHandle.getEJBObject();
         arg = bean.method1(arg);
      }
      catch(Exception e)
      {
         log.error("StatelessSession handle failed", e);
         throw new EJBException("StatelessSession handle failed", e);
      }
      return arg;
   }

   public void createStatefulSessionHandle(String testName)
   {
      log.info("createStatefulSessionHandle");
      try
      {
         StatefulSessionHome home = (StatefulSessionHome) sessionCtx.getEJBHome();
         StatefulSession bean = home.create(testName);
         bean.incCounter();
         Handle handle = bean.getHandle();
         ByteArrayOutputStream baos = new ByteArrayOutputStream(); 
         ObjectOutputStream oos = new ObjectOutputStream(baos); 
         oos.writeObject(handle);
         this.statefulHandle = baos.toByteArray();
      }
      catch(Exception e)
      {
         log.error("Failed to serialize session handle", e);
         throw new EJBException("Failed to serialize session handle", e);
      }
   }

   public void useStatefulSessionHandle()
   {
      log.info("useStatefulSessionHandle");
      try
      {
         ByteArrayInputStream bais = new ByteArrayInputStream(statefulHandle);
         ObjectInputStream ois = new ObjectInputStream(bais);
         Handle handle = (Handle) ois.readObject();
         StatefulSession bean = (StatefulSession) handle.getEJBObject();
         bean.incCounter();
         int count = bean.getCounter();
         log.info("useStatefulSessionHandle, count="+count);
      }
      catch(Exception e)
      {
         log.error("Failed to read session from handle", e);
         throw new EJBException("SFailed to read session from handle", e);
      }
   }

   public void createSessionRef()
      throws RemoteException
   {
      log.info("createSessionRef");
      Handle handle = super.sessionCtx.getEJBObject().getHandle();
      this.sessionRef = new SessionRef(handle);
   }
   public String useSessionRef()
      throws RemoteException
   {
      log.info("useSessionRef");
      Handle handle = sessionRef.getHandle();
      return handle.toString();
   }
}
