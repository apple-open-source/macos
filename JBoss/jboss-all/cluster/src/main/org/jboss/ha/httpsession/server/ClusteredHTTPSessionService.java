/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.httpsession.server;

import java.util.Collection;
import java.util.Iterator;
import java.util.Random;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

import javax.ejb.EJBException;
import javax.naming.InitialContext;
import javax.rmi.PortableRemoteObject;

import org.jboss.system.ServiceMBeanSupport;

import org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSession;
import org.jboss.ha.httpsession.beanimpl.interfaces.LocalClusteredHTTPSession;
import org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionBusiness;
import org.jboss.ha.httpsession.beanimpl.interfaces.ClusteredHTTPSessionHome;
import org.jboss.ha.httpsession.beanimpl.interfaces.LocalClusteredHTTPSessionHome;
import org.jboss.ha.httpsession.interfaces.SerializableHttpSession;

/**
 * JMX Service implementation for ClusteredHTTPSessionServiceMBean
 *
 * @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionServiceMBean
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.8.2.4 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>31. decembre 2001 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class ClusteredHTTPSessionService
   extends ServiceMBeanSupport
   implements ClusteredHTTPSessionServiceMBean
{
   protected final static long CLEANUP_FREQUENCY = 30000; // every 30 seconds
   protected final static int SESSION_ID_BYTES = 16; // We want 16 Bytes for the session-id
   protected final static String SESSION_ID_HASH_ALGORITHM = "MD5";
   protected final static String SESSION_ID_RANDOM_ALGORITHM = "SHA1PRNG";
   protected final static String SESSION_ID_RANDOM_ALGORITHM_ALT = "IBMSecureRandom";


   protected ClusteredHTTPSessionHome httpSessionHome = null;
   protected LocalClusteredHTTPSessionHome localHttpSessionHome = null;
   protected ClusteredHTTPSessionService.CleanupDaemon cleanup = null;
   protected long sessionTimeout = 15*60*1000;
   protected boolean useLocalBean = false;

   protected MessageDigest digest=null;
   protected Random random=null;

   public ClusteredHTTPSessionService()
   {
      super();
   }

   // ClusteredHTTPSessionServiceMBean ----------------------------------------------

   public void setHttpSession (String sessionId, SerializableHttpSession session) throws EJBException
   {
      if (log.isDebugEnabled ())
         log.debug ("setHttpSession called for session: "+ sessionId);

      ClusteredHTTPSessionBusiness bean = null;

      // We first try to find the bean or we create it if it does not yet exist
      //
      try
      {
         if (useLocalBean)
         {
            bean = localHttpSessionHome.findByPrimaryKey (sessionId);
         }
         else
         {
            bean = httpSessionHome.findByPrimaryKey (sessionId);
         }

         // We have one bean: we set its session
         //
         try
         {
            bean.setSession (session);
         }
         catch (Exception e)
         {
            throw new EJBException ("Exception in setHttpSession: " + e.toString ());
         }

      }
      catch (javax.ejb.FinderException fe)
      {
         try
         {
            bean = createSession (sessionId, session);
         }
         catch (Exception e)
         {
            throw new EJBException ("Exception in setHttpSession while creating unexisting session: " + e.toString ());
         }
      }
      catch (Exception e)
      {
         throw new EJBException ("Exception in setHttpSession: " + e.toString ());
      }

   }

   public SerializableHttpSession getHttpSession (String sessionId, ClassLoader tcl)
      throws EJBException
   {
      if (log.isDebugEnabled ())
         log.debug ("getHttpSession called for session: "+ sessionId);

      ClassLoader prevTCL = Thread.currentThread().getContextClassLoader();
      try
      {
         Thread.currentThread().setContextClassLoader(tcl);
         if (useLocalBean)
         {
            LocalClusteredHTTPSession sessionBean = localHttpSessionHome.findByPrimaryKey (sessionId);
            return sessionBean.getSession ();
         }
         else
         {
            ClusteredHTTPSession sessionBean = httpSessionHome.findByPrimaryKey (sessionId);
            return sessionBean.getSession ();
         }
      }
      catch (Exception e)
      {
         throw new EJBException ("Exception in setHttpSession: " + e.toString ());
      }
      finally
      {
         Thread.currentThread().setContextClassLoader(prevTCL);
      }
   }

   public void removeHttpSession (String sessionId) throws EJBException
   {
      if (log.isDebugEnabled ())
         log.debug ("removeHttpSession called for session: "+ sessionId);
      try
      {
         if (useLocalBean)
         {
            localHttpSessionHome.remove (sessionId);
         }
         else
         {
            httpSessionHome.remove (sessionId);
         }
      }
      catch (Exception e)
      {
         throw new EJBException ("Exception in removeHttpSession: " + e.toString ());
      }
   }

   public long getSessionTimeout () { return this.sessionTimeout; }
   public void setSessionTimeout (long miliseconds) { this.sessionTimeout = miliseconds; }

   public synchronized String getSessionId ()
   {
      String id=generateSessionId();
      if (log.isDebugEnabled ())
         log.debug ("getSessionId called: " + id);
      return id;
   }

   public void setUseLocalBean (boolean useLocal)
   {
      int state = this.getState ();
      if (state == this.STARTED || state == this.STARTING)
         return;
      else
         this.useLocalBean = useLocal;
   }

   public boolean getUseLocalBean ()
   {
      return this.useLocalBean;
   }

   // ServiceMBeanSupport overrides ---------------------------------------------------

   protected void startService ()
      throws Exception
   {
      // we (try to) acquire a home reference to the entity bean that stores our sessions
      //
      this.initRefToBean ();
      cleanup = new ClusteredHTTPSessionService.CleanupDaemon ();
      cleanup.start ();
   }

   protected void stopService ()
      throws Exception
   {
      cleanup.stop ();
      httpSessionHome = null;
   }

   protected void initRefToBean () throws Exception
   {
      InitialContext jndiContext = new InitialContext ();
      if (useLocalBean)
      {
         localHttpSessionHome = (LocalClusteredHTTPSessionHome)
            jndiContext.lookup (LocalClusteredHTTPSessionHome.JNDI_NAME);
      }
      else
      {
         Object ref  = jndiContext.lookup (ClusteredHTTPSessionHome.JNDI_NAME);
         httpSessionHome = (ClusteredHTTPSessionHome)
         PortableRemoteObject.narrow (ref, ClusteredHTTPSessionHome.class);
      }
   }

   protected ClusteredHTTPSessionBusiness createSession (String id, SerializableHttpSession session) throws Exception
   {
      try
      {
         ClusteredHTTPSessionBusiness sessionBean = null;
         if (useLocalBean)
         {
            sessionBean = localHttpSessionHome.create (id, session);
         }
         else
         {
            sessionBean = httpSessionHome.create (id, session);
         }
         return sessionBean;
      }
      catch (Exception e)
      {
         throw new EJBException ("Exception in createSession : " + e.toString ());
      }
   }

   /**
     Generate a session-id that is not guessable
     @return generated session-id
     */
   protected synchronized String generateSessionId()
   {
        if (this.digest==null) {
	  this.digest=getDigest();
	}

	if (this.random==null) {
	   this.random=getRandom();
	}

	byte[] bytes=new byte[SESSION_ID_BYTES];

	// get random bytes
	this.random.nextBytes(bytes);

	// Hash the random bytes
	bytes=this.digest.digest(bytes);

        // Render the result as a String of hexadecimal digits
	return encode(bytes);
   }

   /**
    Encode the bytes into a String with a slightly modified Base64-algorithm
    This code was written by Kevin Kelley <kelley@ruralnet.net>
    and adapted by Thomas Peuss <jboss@peuss.de>
    @param data The bytes you want to encode
    @return the encoded String
   */
   protected String encode(byte[] data)
   {
    char[] out = new char[((data.length + 2) / 3) * 4];
    char[] alphabet =  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-*".toCharArray();

    //
    // 3 bytes encode to 4 chars.  Output is always an even
    // multiple of 4 characters.
    //
    for (int i=0, index=0; i<data.length; i+=3, index+=4) {
        boolean quad = false;
        boolean trip = false;

        int val = (0xFF & (int) data[i]);
        val <<= 8;
        if ((i+1) < data.length) {
            val |= (0xFF & (int) data[i+1]);
            trip = true;
        }
        val <<= 8;
        if ((i+2) < data.length) {
            val |= (0xFF & (int) data[i+2]);
            quad = true;
        }
        out[index+3] = alphabet[(quad? (val & 0x3F): 64)];
        val >>= 6;
        out[index+2] = alphabet[(trip? (val & 0x3F): 64)];
        val >>= 6;
        out[index+1] = alphabet[val & 0x3F];
        val >>= 6;
        out[index+0] = alphabet[val & 0x3F];
    }
    return new String(out);
   }

   /**
    get a random-number generator
    @return a random-number generator
   */
   protected synchronized Random getRandom()
   {
      long seed;
      Random random=null;

      // Mix up the seed a bit
      seed=System.currentTimeMillis();
      seed^=Runtime.getRuntime().freeMemory();

      try {
         random=SecureRandom.getInstance(SESSION_ID_RANDOM_ALGORITHM);
      }
      catch (NoSuchAlgorithmException e)
      {
         try
         {
            random=SecureRandom.getInstance(SESSION_ID_RANDOM_ALGORITHM_ALT);
         }
         catch (NoSuchAlgorithmException e_alt)
         {
           log.error("Could not generate SecureRandom for session-id randomness",e);
           log.error("Could not generate SecureRandom for session-id randomness",e_alt);
           return null;
         }
      }

      // set the generated seed for this PRNG
      random.setSeed(seed);

      return random;
   }

   /**
     get a MessageDigest hash-generator
     @return a hash generator
     */
   protected synchronized MessageDigest getDigest()
   {
      MessageDigest digest=null;

      try {
	 digest=MessageDigest.getInstance(SESSION_ID_HASH_ALGORITHM);
      } catch (NoSuchAlgorithmException e) {
	 log.error("Could not generate MessageDigest for session-id hashing",e);
	 return null;
      }

      return digest;
   }

   protected class CleanupDaemon
      implements Runnable
   {
      protected boolean stopping = false;

      public CleanupDaemon () throws Exception
      {
      }

      public void start ()
      {
         stopping = false;
         Thread t = new Thread (this, "ClusteredHTTPSessionService - CleanupDaemon");
         t.start ();
      }

      public void stop ()
      {
         stopping = true;
      }

      public void run ()
      {
         while (!stopping)
         {
            try
            {
               // we get all beans and only check if they have timeouted. If we
               // don't ask for the HTTPSession content attribute, this will *not*
               // deserialize their content!
               //
               Collection allBeans = null;

               if (useLocalBean)
                  allBeans = localHttpSessionHome.findAll ();
               else
                  allBeans = httpSessionHome.findAll ();

               Iterator iter = allBeans.iterator ();
               long now = System.currentTimeMillis ();
               while (iter.hasNext ())
               {
                  try
                  {
                     ClusteredHTTPSessionBusiness sessionBean = (ClusteredHTTPSessionBusiness)iter.next ();
                     long lastAccess = sessionBean.getLastAccessedTime ();

                     if ( ( now - lastAccess) > sessionTimeout )
                     {
                        if (useLocalBean)
                           ((LocalClusteredHTTPSession)sessionBean).remove ();
                        else
                           ((ClusteredHTTPSession)sessionBean).remove ();
                     }
                  }
                  catch(Exception notImportant)
                  {
                     log.debug(notImportant);
                  }


                  if (stopping)
                     return;
               }
            }
            catch (Exception e)
            {
               log.info ("unexpected exception while removing orphean replicated sessions", e);
            }
            finally
            {
               if (!stopping)
                  // and we sleep a while...
                  //
                  try
                  {
                     synchronized (this)
                     {
                        this.wait (CLEANUP_FREQUENCY);
                     }
                  }
                  catch (InterruptedException gameOver)
                  {
                     stopping = true;
                     return;
                  }

            }
         }
      }
   }
}
