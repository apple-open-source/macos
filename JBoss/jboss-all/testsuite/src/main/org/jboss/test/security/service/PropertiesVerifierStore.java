package org.jboss.test.security.service;

import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.IOException;
import java.math.BigInteger;
import java.net.URL;
import java.security.KeyException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Properties;
import javax.naming.InitialContext;
import javax.naming.Name;

import org.jboss.naming.NonSerializableFactory;
import org.jboss.security.Util;
import org.jboss.security.srp.SRPConf;
import org.jboss.security.srp.SRPVerifierStore;
import org.jboss.security.srp.SRPVerifierStore.VerifierInfo;
import org.jboss.system.ServiceMBeanSupport;

/** The PropertiesVerifierStore service is a SRPVerifierStore implementation
 that obtains the username and password info from a properties file and then
 creates an in memory SRPVerifierStore.

@author Scott.Stark@jboss.org
@version $Revision: 1.1.4.2 $
*/
public class PropertiesVerifierStore extends ServiceMBeanSupport
   implements PropertiesVerifierStoreMBean, SRPVerifierStore
{
   private String jndiName = "srp/DefaultVerifierSource";
   private HashMap storeMap = new HashMap();
   private Thread addUserThread;

   /** Creates a new instance of PropertiesVerifierStore */
   public PropertiesVerifierStore()
   {
   }

   /** Get the jndi name for the SRPVerifierSource implementation binding.
   */
   public String getJndiName()
   {
     return jndiName;
   }
   /** set the jndi name for the SRPVerifierSource implementation binding.
   */
   public void setJndiName(String jndiName)
   {
      this.jndiName = jndiName;
   }

   protected void startService() throws Exception
   {
      // Make sure the security utility class is initialized
      Util.init();

      // Find the users.properties file
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      URL users = loader.getResource("users.properties");
      if( users == null )
         throw new FileNotFoundException("Failed to find users.properties resource");
      InputStream is = users.openStream();
      final Properties userPasswords = new Properties();
      userPasswords.load(is);
      is.close();
      addUserThread = new Thread("AddUsers")
      {
         public void run()
         {
            Iterator keys = userPasswords.keySet().iterator();
            while( keys.hasNext() )
            {
               String username = (String) keys.next();
               char[] password = userPasswords.getProperty(username).toCharArray();
               String cipherAlgorithm = "Blowfish";
               String hashAlgorithm = "SHA_Interleave";
               addUser(username, password, cipherAlgorithm, hashAlgorithm);
               log.info("Added user: "+username);
            }
         }
      };
      addUserThread.start();

      // Bind a reference to the SRPVerifierStore using NonSerializableFactory
      InitialContext ctx = new InitialContext();
      Name name = ctx.getNameParser("").parse(jndiName);
      NonSerializableFactory.rebind(name, this, true);
      log.debug("Bound SRPVerifierStore at "+jndiName);
   }
   protected void stopService() throws Exception
   {
      InitialContext ctx = new InitialContext();
      NonSerializableFactory.unbind(jndiName);
      ctx.unbind(jndiName);
      log.debug("Unbound SRPVerifierStore at "+jndiName);
   }

   public VerifierInfo getUserVerifier(String username) throws KeyException, IOException
   {
      if( addUserThread != null )
      {
         try
         {
            addUserThread.join();
            addUserThread = null;
         }
         catch(InterruptedException e)
         {
         }
      }
      VerifierInfo info = (VerifierInfo) storeMap.get(username);
      return info;
   }
   public void setUserVerifier(String username, VerifierInfo info) throws IOException
   {
      throw new IOException("PropertiesVerifierStore is read only");
   }

   public void verifyUserChallenge(String username, Object auxChallenge)
         throws SecurityException
   {
   }

   private void addUser(String username, char[] password, String cipherAlgorithm,
      String hashAlgorithm)
   {
      VerifierInfo info = new VerifierInfo();
      info.username = username;
      // Create a random salt
      long r = Util.nextLong();
      String rs = Long.toHexString(r);
      info.salt = rs.getBytes();
      BigInteger g = SRPConf.getDefaultParams().g();
      BigInteger N = SRPConf.getDefaultParams().N();
      info.cipherAlgorithm = cipherAlgorithm;
      info.hashAlgorithm = hashAlgorithm;

      info.verifier = Util.calculateVerifier(username, password, info.salt, N, g);
      info.g = g.toByteArray();
      info.N = N.toByteArray();
      storeMap.put(username, info);
   }
}
