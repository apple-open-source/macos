package org.jboss.jmx.adaptor.html;

import java.io.IOException;
import java.io.StringReader;
import java.io.BufferedReader;
import java.util.Properties;
import java.util.Vector;
import java.util.Enumeration;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.RequestDispatcher;
import javax.naming.InitialContext;
import javax.naming.Context;
import javax.naming.NamingException;
import javax.naming.NamingEnumeration;
import javax.naming.NameClassPair;
import javax.management.ObjectName;

import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.jmx.adaptor.control.AddressPort;
import org.jboss.jmx.adaptor.rmi.RMIAdaptor;
import org.jboss.logging.Logger;
import org.jnp.interfaces.NamingContext;

/** A servlet that provides the cluster view bootstrap index for the
 * jmx-console cluster frame.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class ClusteredConsoleServlet  extends HttpServlet
{
   private static Logger log = Logger.getLogger(ClusteredConsoleServlet.class);

   private static final String ACTION_PARAM             = "action";
   private static final String CLUSTER_BOOTSTRAP_ACTION = "bootstrap";
   private static final String CLUSTER_INDEX_ACTION     = "index";

   private static final String BOOTSTRAP_PARAM         = "bootstrap";
   private static final String PARTITION_PARAM         = "partition";
   private static final String HOSTNAME_PARAM          = "hostname";
   private static final String PORT_PARAM              = "port";
   private static final String DISCOVERY_GROUP_PARAM   = "discoveryGroup";
   private static final String DISCOVERY_TIMEOUT_PARAM = "discoveryTimeout";

   private String jgProps;

   /** Creates a new instance of HtmlAdaptor */
   public ClusteredConsoleServlet()
   {
   }

   public void init(ServletConfig config) throws ServletException
   {
      super.init(config);

      jgProps = config.getInitParameter("jgProps");
      if( jgProps == null )
         throw new ServletException("No jgProps init-param specified");
      StringBuffer trimedProps = new StringBuffer();
      StringReader sr = new StringReader(jgProps);
      BufferedReader br = new BufferedReader(sr);
      String protocol = null;
      try
      {
         while( (protocol = br.readLine()) != null )
            trimedProps.append(protocol.trim());
      }
      catch(IOException e)
      {
         throw new ServletException("Failed to process jgProps", e);
      }
      jgProps = trimedProps.toString();
      log.debug("Using jbPropgs: "+jgProps);
   }

   public void destroy()
   {
   }

   protected void doGet(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      processRequest(request, response);
   }
   protected void doPost(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      processRequest(request, response);
   }

   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String action = request.getParameter(ACTION_PARAM);

      if( action == null )
         action = CLUSTER_INDEX_ACTION;

      if( action.equals(CLUSTER_INDEX_ACTION) )
         clusterIndex(request, response);
      else if( action.equals(CLUSTER_BOOTSTRAP_ACTION) )
         clusterBootstrap(request, response);
   }


   /** cluster index view
    */
   private void clusterIndex(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      try
      {
         // Query for the membership of the partition cluster
		 String[] hosts = {};
         request.setAttribute("partition", "none");
         request.setAttribute("partitionHosts", hosts);
         RequestDispatcher rd = this.getServletContext().getRequestDispatcher("/cluster/clusterView.jsp");
         rd.forward(request, response);
      }
      catch(Exception e)
      {
         log.debug("Failed to get partition view", e);
         response.sendError(HttpServletResponse.SC_NO_CONTENT, "No partition view found");
      }
   }


   /** bootstrap cluster node view
    */
   private void clusterBootstrap(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String bootstrap = request.getParameter(BOOTSTRAP_PARAM);
      log.debug("processRequest, parameters:");
      Enumeration params = request.getParameterNames();
      while( params.hasMoreElements() )
      {
         String name = (String) params.nextElement();
         log.debug(name+"="+request.getParameter(name));
      }

      if( bootstrap == null )
         bootstrap = "discovery";
      String hostname = request.getParameter(HOSTNAME_PARAM);
      if( hostname == null || hostname.equalsIgnoreCase("localhost") )
         hostname = request.getServerName();
      String partition = request.getParameter(PARTITION_PARAM);
      String port = request.getParameter(PORT_PARAM);
      String discoveryGroup = request.getParameter(DISCOVERY_GROUP_PARAM);
      String discoveryTimeout = request.getParameter(DISCOVERY_TIMEOUT_PARAM);

      log.debug("bootstrap: "+bootstrap);
      String[] hosts = {};
      Properties env = new Properties();
      try
      {
         if( bootstrap.equals("discovery") )
         {
            if( partition != null && partition.length() > 0 )
               env.setProperty(NamingContext.JNP_PARTITION_NAME, partition);
            if( port != null && port.length() > 0 )
               env.setProperty(NamingContext.JNP_DISCOVERY_PORT, port);
            if( discoveryGroup != null && discoveryGroup.length() > 0 )
               env.setProperty(NamingContext.JNP_DISCOVERY_GROUP, discoveryGroup);
            if( discoveryTimeout != null && discoveryTimeout.length() > 0 )
               env.setProperty(NamingContext.JNP_DISCOVERY_TIMEOUT, discoveryTimeout);
            hosts = discoverHosts(env);
         }
         else if( bootstrap.equals("byhost") )
         {
            queryHost(hostname, port, env);
         }
         else
         {
            throw new ServletException("Unkown bootstrap mode specified: "+bootstrap);
         }
      }
      catch(Exception e)
      {
         throw new ServletException("Failed to bootstrap hosts", e);
      }

      try
      {
         // Query for the membership of the partition cluster
         partition = env.getProperty(NamingContext.JNP_PARTITION_NAME);
         request.setAttribute("partition", partition);
         request.setAttribute("partitionHosts", hosts);
         RequestDispatcher rd = this.getServletContext().getRequestDispatcher("/cluster/clusterView.jsp");
         rd.forward(request, response);
      }
      catch(Exception e)
      {
         log.debug("Failed to get partition view", e);
         response.sendError(HttpServletResponse.SC_NO_CONTENT, "No partition view found");
      }
   }


   private String[] discoverHosts(Properties env)
      throws NamingException, IOException
   {
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");

      String[] hosts = {};
      log.debug("Querying HAJNDI: "+env);
      InitialContext ctx = new InitialContext(env);
      String partitionName = env.getProperty(NamingContext.JNP_PARTITION_NAME);
      if( partitionName != null )
      {
         String partitionJndiName = "/HAPartition/" + partitionName;
         HAPartition partition = (HAPartition) ctx.lookup(partitionJndiName);
         Vector view = partition.getCurrentView();
         log.debug("Found HAPartition: "+partitionName);
         hosts = new String[view.size()];
         for(int v = 0; v < view.size(); v ++)
         {
            Object addr = view.get(v);
            log.debug(addr);
            hosts[v] = addr.toString();
         }
      }
      else
      {
         NamingEnumeration iter = ctx.list("/HAPartition");
         while( iter.hasMore() )
         {
            NameClassPair pair = (NameClassPair) iter.next();
            partitionName = pair.getName();
            String partitionJndiName = "/HAPartition/" + partitionName;
            HAPartition partition = (HAPartition) ctx.lookup(partitionJndiName);
            env.setProperty(NamingContext.JNP_PARTITION_NAME, partitionName);
            Vector view = partition.getCurrentView();
            log.debug("Found HAPartition: "+partitionName);
            hosts = new String[view.size()];
            for(int v = 0; v < view.size(); v ++)
            {
               Object addr = view.get(v);
               AddressPort ap = AddressPort.getMemberAddress(addr);
               log.debug(ap);
               hosts[v] = ap.getHostAddress();
            }
            break;
         }
      }

      return hosts;
   }

   private String[] queryHost(String hostname, String port, Properties env)
      throws Exception
   {
      String[] hosts = {};
      env.setProperty(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
      env.setProperty(Context.PROVIDER_URL, hostname+":"+port);
      InitialContext ctx = new InitialContext(env);
      NamingEnumeration iter = ctx.list("/HAPartition");
      String partitionName = null;
      while( iter.hasMore() )
      {
         NameClassPair pair = (NameClassPair) iter.next();
         partitionName = pair.getName();
         break;
      }
      if( partitionName == null )
         throw new NamingException("Failed to find any parition");
      env.setProperty(NamingContext.JNP_PARTITION_NAME, partitionName);

      RMIAdaptor adaptor = (RMIAdaptor) ctx.lookup("jmx/rmi/RMIAdaptor");
      ObjectName clusterPartition = new ObjectName("jboss:service="+partitionName);
      Vector view = (Vector) adaptor.getAttribute(clusterPartition, "CurrentView");
      log.debug("Found ClusterPartition: "+clusterPartition);
      hosts = new String[view.size()];
      for(int v = 0; v < view.size(); v ++)
      {
         Object addr = view.get(v);
         AddressPort ap = AddressPort.getMemberAddress(addr);
         log.debug(ap);
         hosts[v] = ap.getHostAddress();
      }
      return hosts;
   }
}
