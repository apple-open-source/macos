package org.jboss.ha.framework.server.util;

import java.util.ArrayList;
import java.util.Hashtable;

import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.naming.Context;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.logging.Logger;
import  org.jboss.ha.framework.server.util.TopologyMonitorService.AddressPort;

/** A utility MBean that can be used as the trigger target of the
 * TopologyMonitorService to probe the state of JNDI on the cluster nodes.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class PingJndi extends ServiceMBeanSupport
   implements PingJndiMBean
{
   private String urlPrefix;
   private String urlSuffix;
   private String urlPattern;
   private String[] lookupNames;

   /** Get the names of JNDI bindings that should be queried on each host
    * @return the array of target names to test
    * @jmx:managed-attribute
    */
   public String[] getLookupNames()
   {
      return lookupNames;
   }
   /** Set the names of JNDI bindings that should be queried on each host
    * @param names
    * @jmx:managed-attribute
    */
   public void setLookupNames(String[] names)
   {
      this.lookupNames = names;
   }

   /** Get the Context.PROVIDER_URL regular expression.
    * @return the regular expression containing the host, for example
    * 'jnp://(host):1099/'
    * @jmx:managed-attribute
    */
   public String getProviderURLPattern()
   {
      return urlPattern;
   }

   /** Set the regular expression containing the hostname/IP address of
    * the JNDI provider. This expression is used to build the JNDI
    * Context.PROVIDER_URL for each node in the cluster. The expression
    * should contain a "(host)" component that will be replaced with the
    * cluster node hostname.
    *
    * @param regex the regular expression containing the host, for example
    * 'jnp://(host):1099/'
    * @jmx:managed-attribute
    */
   public void setProviderURLPattern(String regex)
   {
      this.urlPattern = regex;
      this.urlPrefix = regex;
      this.urlSuffix = "";
      String hostExp = "{host}";
      int hostIndex = regex.indexOf(hostExp);
      if( hostIndex >= 0 )
      {
         urlPrefix = regex.substring(0, hostIndex);
         int endIndex = hostIndex + hostExp.length();
         urlSuffix = regex.substring(endIndex);
      }
   }

   /** The TopologyMonitorService trigger callback operation.
    *
    * @param removed ArrayList<AddressPort> of nodes that were removed
    * @param added ArrayList<AddressPort> of nodes that were added
    * @param members ArrayList<AddressPort> of nodes currently in the cluster
    * @param logCategoryName the log4j category name used by the
    * TopologyMonitorService. This is used for logging to integrate with
    * the TopologyMonitorService output.
    */
   public void membershipChanged(ArrayList removed, ArrayList added,
      ArrayList members, String logCategoryName)
   {
      log.debug("membershipChanged");
      Logger tmsLog = Logger.getLogger(logCategoryName);
      Hashtable localEnv = null;
      try
      {
         InitialContext localCtx = new InitialContext();
         localEnv = localCtx.getEnvironment();
      }
      catch(NamingException e)
      {
         tmsLog.error("Failed to obtain InitialContext env", e);
         return;
      }

      tmsLog.info("Checking removed hosts JNDI binding");
      doLookups(localEnv, tmsLog, removed);
      tmsLog.info("Checking added hosts JNDI binding");
      doLookups(localEnv, tmsLog, added);
      tmsLog.info("Checking members hosts JNDI binding");
      doLookups(localEnv, tmsLog, members);
   }

   private void doLookups(Hashtable localEnv, Logger tmsLog, ArrayList nodes)
   {
      for(int n = 0; n < nodes.size(); n ++)
      {
         AddressPort addrInfo = (AddressPort) nodes.get(n);
         String providerURL = urlPrefix + addrInfo.getHostName() + urlSuffix;
         Hashtable env = new Hashtable(localEnv);
         env.put(Context.PROVIDER_URL, providerURL);
         tmsLog.info("Checking names on: "+addrInfo);
         try
         {
            InitialContext ctx = new InitialContext(env);
            for(int s = 0; s < lookupNames.length; s ++)
            {
               String name = lookupNames[s];
               Object value = ctx.lookup(name);
               tmsLog.info("lookup("+name+"): "+value);
            }
         }
         catch(Exception e)
         {
            tmsLog.error("Failed lookups on: "+addrInfo, e);
         }
      }
   }
}
