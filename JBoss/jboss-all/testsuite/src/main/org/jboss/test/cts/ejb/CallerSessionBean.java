package org.jboss.test.cts.ejb;

import java.rmi.RemoteException;
import java.rmi.ServerException;
import java.util.Properties;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.apache.log4j.Logger;
import org.jboss.test.cts.interfaces.CallerSession;
import org.jboss.test.cts.interfaces.CallerSessionHome;
import org.jboss.test.cts.interfaces.ReferenceTest;
import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.mx.loading.ClassLoaderUtils;

/** The stateless session bean implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class CallerSessionBean
      extends SessionSupport
{
   private static Logger log = Logger.getLogger(CallerSessionBean.class);

   private CallerSessionHome cachedHome;

   public void ejbCreate() throws CreateException
   {
   }

   public void simpleCall2(boolean isCaller) throws RemoteException
   {
      log.info("simpleCall2, isCaller: "+isCaller);
      // If this is the callee just return
      if( isCaller == false )
         return;

      // Call the second deployment instance
      CallerSessionHome home = null;
      CallerSession callee = null;

      try
      {
         home = lookupHome("ejbcts2/CalleeSessionHome");
         callee = home.create();
      }
      catch(NamingException e)
      {
         throw new ServerException("Failed to lookup CalleeHome", e);
      }
      catch(CreateException e)
      {
         throw new ServerException("Failed to create Callee", e);
      }

      callee.simpleCall(false);
   }

   public void simpleCall(boolean isCaller) throws RemoteException
   {
      log.info("simpleCall, isCaller: "+isCaller);
      // If this is the callee just return
      if( isCaller == false )
         return;

      // Call the second deployment instance
      CallerSession callee = null;
      try
      {
         cachedHome = lookupHome("ejbcts2/CalleeSessionHome");
         callee = cachedHome.create();
      }
      catch(NamingException e)
      {
         throw new ServerException("Failed to lookup CalleeHome", e);
      }
      catch(CreateException e)
      {
         throw new ServerException("Failed to create Callee", e);
      }
      catch(Throwable e)
      {
         log.error("Unexpected error", e);
         throw new ServerException("Unexpected error"+e.getMessage());
      }

      callee.simpleCall2(false);
   }

   /** Lookup the cts.jar/CalleeHome binding and invoke
    *
    * @throws RemoteException
    */
   public void callByValueInSameJar() throws RemoteException
   {
      // Call the second deployment instance
      CallerSession callee = null;
      try
      {
         cachedHome = lookupHome("ejbcts/CalleeSessionHome");
         callee = cachedHome.create();
      }
      catch(NamingException e)
      {
         throw new ServerException("Failed to lookup CalleeHome", e);
      }
      catch(CreateException e)
      {
         throw new ServerException("Failed to create Callee", e);
      }
      catch(Throwable e)
      {
         log.error("Unexpected error", e);
         throw new ServerException("Unexpected error"+e.getMessage());
      }

      ReferenceTest test = new ReferenceTest();
      callee.validateValueMarshalling(test);
   }

   public void validateValueMarshalling(ReferenceTest test)
   {
      boolean wasSerialized = test.getWasSerialized();
      log.info("validateValueMarshalling, testWasSerialized: "+wasSerialized);
      if( wasSerialized == false )
         throw new EJBException("ReferenceTest was not serialized");
   }

   private CallerSessionHome lookupHome(String ejbName) throws NamingException
   {
      CallerSessionHome home = null;
      Properties env = new Properties();
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
      env.setProperty(Context.OBJECT_FACTORIES, "org.jboss.naming:org.jnp.interfaces");
      env.setProperty(Context.PROVIDER_URL, "localhost:1099");

      InitialContext ctx = new InitialContext(env);
      log.info("looking up: "+ejbName);
      Object ref = ctx.lookup(ejbName);
      StringBuffer buffer = new StringBuffer("JNDI CallerSessionHome.class: ");
      ClassLoaderUtils.displayClassInfo(ref.getClass(), buffer);
      log.info(buffer.toString());
      buffer.setLength(0);
      buffer.append("Session CallerSessionHome.class: ");
      ClassLoaderUtils.displayClassInfo(CallerSessionHome.class, buffer);
      log.info(buffer.toString());

      home = (CallerSessionHome) ref;
      return home;
   }
}
