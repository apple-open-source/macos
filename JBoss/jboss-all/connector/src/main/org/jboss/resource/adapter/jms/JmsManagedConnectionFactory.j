/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.resource.adapter.jms;

import java.util.Set;
import java.util.Iterator;

import java.io.PrintWriter;

import javax.security.auth.Subject;

import javax.resource.ResourceException;

import javax.resource.spi.ManagedConnectionFactory;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.SecurityException;
import javax.resource.spi.IllegalStateException;

import javax.resource.spi.security.PasswordCredential;

import org.jboss.jms.jndi.JMSProviderAdapter;

import org.jboss.logging.Logger;

/**
 * ???
 *
 * Created: Sat Mar 31 03:08:35 2001
 *
 * @author <a href="mailto:peter.antman@tim.se">Peter Antman</a>.
 * @version $Revision: 1.4 $
 */
public class JmsManagedConnectionFactory
   implements ManagedConnectionFactory
{
   private static final Logger log = Logger.getLogger(JmsManagedConnection.class);

   /** Settable attributes in ra.xml */
   private JmsMCFProperties mcfProperties = new JmsMCFProperties();

   /** For local access. */
   private JMSProviderAdapter adapter;
   
   public JmsManagedConnectionFactory() {
      // empty
   }
   
   /**
    * Create a "non managed" connection factory. No appserver involved
    */
   public Object createConnectionFactory() throws ResourceException 
   {
      return createConnectionFactory(null);
   }

   /**
    * Create a ConnectionFactory with appserver hook
    */ 
   public Object createConnectionFactory(ConnectionManager cxManager)
      throws ResourceException 
   {
      Object cf = new JmsConnectionFactoryImpl(this, cxManager);
      
      if (log.isTraceEnabled()) {
         log.trace("Created connection factory: " + cf + ", using connection manager: " + cxManager);
      }
      
      return cf;
   }
    
   /**
    * Create a new connection to manage in pool
    */
   public ManagedConnection createManagedConnection(Subject subject, 
                                                    ConnectionRequestInfo info) 
      throws ResourceException 
   {
      boolean trace = log.isTraceEnabled();
      
      info = getInfo(info);
      if (trace) log.trace("connection request info: " + info);
      
      JmsCred cred = JmsCred.getJmsCred(this,subject, info);
      if (trace) log.trace("jms credentials: " + cred);
      
      // OK we got autentication stuff
      JmsManagedConnection mc =
         new JmsManagedConnection(this, info, cred.name, cred.pwd);

      if (trace) log.trace("created new managed connection: " + mc);
      
      // Set default logwriter according to spec

      // 
      // jason: screw the logWriter stuff for now it sucks ass
      //
      
      return mc;
   }

   /**
    * Match a set of connections from the pool
    */
   public ManagedConnection matchManagedConnections(Set connectionSet,
                                                    Subject subject,
                                                    ConnectionRequestInfo info) 
      throws ResourceException
   {
      boolean trace = log.isTraceEnabled();
      
      // Get cred
      info = getInfo(info);
      JmsCred cred = JmsCred.getJmsCred(this, subject, info);

      if (trace) log.trace("Looking for connection matching credentials: " + cred);
      
      // Traverse the pooled connections and look for a match, return first found
      Iterator connections = connectionSet.iterator();
      
      while (connections.hasNext()) {
         Object obj = connections.next();
	    
         // We only care for connections of our own type
         if (obj instanceof JmsManagedConnection) {
            // This is one from the pool
            JmsManagedConnection mc = (JmsManagedConnection) obj;
		
            // Check if we even created this on
            ManagedConnectionFactory mcf =
               mc.getManagedConnectionFactory();
		
            // Only admit a connection if it has the same username as our
            // asked for creds

            // FIXME, Here we have a problem, jms connection 
            // may be anonymous, have a user name
		
            if ((mc.getUserName() == null || 
                 (mc.getUserName() != null && 
                  mc.getUserName().equals(cred.name))) && mcf.equals(this))
            {
               // Now check if ConnectionInfo equals
               if (info.equals( mc.getInfo() )) {

                  if (trace) log.trace("Found matching connection: " + mc);
                  
                  return mc;
               }
            }
         }
      }

      if (trace) log.trace("No matching connection was found");
      
      return null;
   }

   public void setLogWriter(PrintWriter out)
      throws ResourceException
   {
      // 
      // jason: screw the logWriter stuff for now it sucks ass
      //
   }

   public PrintWriter getLogWriter() throws ResourceException {
      // 
      // jason: screw the logWriter stuff for now it sucks ass
      //

      return null;
   }

   /**
    * Checks for equality ower the configured properties.
    */
   public boolean equals(Object obj) {
      if (obj == null) return false;
      if (obj instanceof JmsManagedConnectionFactory) {
	 return mcfProperties.equals( ((JmsManagedConnectionFactory)obj).getProperties());
      }
      else {
         return false;
      }
   }

   public int hashCode() {
      return mcfProperties.hashCode();
   }

   // --- Connfiguration API ---
   
   public void setJmsProviderAdapterJNDI(String jndi) {
      mcfProperties.setProviderJNDI(jndi);
   }
    
   public String getJmsProviderAdapterJNDI() {
      return mcfProperties.getProviderJNDI();
   }

   /**
    * Set userName, null by default.
    */
   public void setUserName(String userName) {
      mcfProperties.setUserName(userName);
   }

   /**
    * Get userName, may be null.
    */ 
   public String getUserName() {
      return mcfProperties.getUserName();
   }
   
   /**
    * Set password, null by default.
    */
   public void setPassword(String password) {
      mcfProperties.setPassword(password);
   }
   /**
    * Get password, may be null.
    */
   public String getPassword() {
      return  mcfProperties.getPassword();
   }

   /**
    * Set the default session typ
    *
    * @param type either javax.jms.Topic or javax.jms.Queue
    * 
    * @exception ResourceException if type was not a valid type.
    */
   public void setSessionDefaultType(String type) throws ResourceException {
      mcfProperties.setSessionDefaultType(type);
   }

   public String getSessionDefaultType() {
      return mcfProperties.getSessionDefaultType();
   }

   /**
    * For local access
    */
   public void setJmsProviderAdapter(final JMSProviderAdapter adapter) {
      this.adapter = adapter;
   }
    
   public JMSProviderAdapter getJmsProviderAdapter() {
      return adapter;
   }

   private ConnectionRequestInfo getInfo(ConnectionRequestInfo info) {   
      if (info == null) {
	 // Create a default one
	 return new JmsConnectionRequestInfo(mcfProperties);
      }
      else {
	 // Fill the one with any defaults
	 ((JmsConnectionRequestInfo)info).setDefaults(mcfProperties);
	 return info;
      }
   }
   
   //---- MCF to MCF API
   
   protected JmsMCFProperties getProperties() {
      return mcfProperties;
   }
}
