package org.jboss.deployment;

import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.util.Iterator;
import java.util.HashMap;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.LinkRef;
import javax.naming.NamingException;

import org.jboss.ejb.EjbUtil;
import org.jboss.metadata.ClientMetaData;
import org.jboss.metadata.EnvEntryMetaData;
import org.jboss.metadata.EjbRefMetaData;
import org.jboss.metadata.ResourceEnvRefMetaData;
import org.jboss.metadata.ResourceRefMetaData;
import org.jboss.metadata.XmlFileLoader;
import org.jboss.naming.Util;

import org.w3c.dom.Element;

/** An XMBean resource implementation of a deployer for j2ee application
 * client jars 
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ClientDeployer extends SubDeployerSupport
{
   protected void startService() throws Exception
   {
      // register with MainDeployer
      super.startService();
   }

   /**
    * Implements the template method in superclass. This method stops all the
    * applications in this server.
    */
   protected void stopService() throws Exception
   {
      // deregister with MainDeployer
      super.stopService();
   }

   /** This method looks to the deployment for a META-INF/application-client.xml
    * descriptor to identify a j2ee client jar.
    *
    * @param di The deployment info instance for the jar
    * @return true if the deployment is a j2ee client jar, false otherwise
    */
   public boolean accepts(DeploymentInfo di)
   {
      // To be accepted the deployment's root name must end in jar
      String urlStr = di.url.getFile();
      if( !urlStr.endsWith("jar") && !urlStr.endsWith("jar/") )
      {
         return false;
      }

      // However the jar must also contain an META-INF/application-client.xml
      boolean accepts = false;
      try
      {
         URL dd = di.localCl.findResource("META-INF/application-client.xml");
         if (dd != null)
         {
            log.debug("Found a META-INF/application-client.xml file, di: "+di);
            accepts = true;
         }
      }
      catch( Exception ignore )
      {
      }

      return accepts;
   }

   /** Parse the application-client.xml and jboss-client.xml descriptors.
    */
   public void create(DeploymentInfo di) throws DeploymentException
   {
      InputStream in = di.localCl.getResourceAsStream("META-INF/application-client.xml");
      if( in == null )
         throw new DeploymentException("No META-INF/application-client.xml found");

      ClientMetaData metaData = null;
      try
      {
         XmlFileLoader xfl = new XmlFileLoader(true);
         Element appClient = xfl.getDocument(in, "META-INF/application.xml").getDocumentElement();
         in.close();
         metaData = new ClientMetaData();
         metaData.importClientXml(appClient);
         di.metaData = metaData;

         // Look for a jboss-client.xml descriptor
         in = di.localCl.getResourceAsStream("META-INF/jboss-client.xml");
         if( in != null )
         {
            xfl = new XmlFileLoader(true);
            Element jbossClient = xfl.getDocument(in, "META-INF/jboss-client.xml").getDocumentElement();
            in.close();
            metaData.importJbossClientXml(jbossClient);
         }
      }
      catch (IOException e)
      {
         throw new DeploymentException("Failed to parse metadata", e);
      }

      try
      {
         setupEnvironment(di, metaData);
      }
      catch (Exception e)
      {
         throw new DeploymentException("Failed to setup client ENC", e);
      }

      super.create(di);
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'start' logic.
    *
    * This method issues a JMX notification of type SubDeployer.START_NOTIFICATION.
    */
   public void start(DeploymentInfo di) throws DeploymentException
   {
      super.start(di);
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'stop' logic.
    *
    * This method issues a JMX notification of type SubDeployer.START_NOTIFICATION.
    */
   public void stop(DeploymentInfo di) throws DeploymentException
   {
      super.stop(di);
   }

   /**
    * Sub-classes should override this method to provide
    * custom 'destroy' logic.
    *
    * This method issues a JMX notification of type SubDeployer.DESTROY_NOTIFICATION.
    */
   public void destroy(DeploymentInfo di) throws DeploymentException
   {
      // Setup a JNDI context which contains
      ClientMetaData metaData = (ClientMetaData) di.metaData;
      String appClientName = metaData.getJndiName();
      try
      {
         InitialContext iniCtx = new InitialContext();
         Util.unbind(iniCtx, appClientName);
      }
      catch(NamingException e)
      {
         throw new DeploymentException("Failed to remove client ENC", e);
      }
      super.destroy(di);
   }

   private void setupEnvironment(DeploymentInfo di, ClientMetaData metaData)
      throws Exception
   {
      // Setup a JNDI context which contains
      String appClientName = metaData.getJndiName();
      InitialContext iniCtx = new InitialContext();
      Context envCtx = Util.createSubcontext(iniCtx, appClientName);
      log.debug("Creating client ENC binding under: "+appClientName);
      // Bind environment properties
      Iterator enum = metaData.getEnvironmentEntries().iterator();
      while(enum.hasNext())
      {
         EnvEntryMetaData entry = (EnvEntryMetaData)enum.next();
         log.debug("Binding env-entry: "+entry.getName()+" of type: "+
            entry.getType()+" to value:"+entry.getValue());
         EnvEntryMetaData.bindEnvEntry(envCtx, entry);
      }

      // Bind EJB references
      HashMap ejbRefs = metaData.getEjbReferences();
      enum = ejbRefs.values().iterator();
      while(enum.hasNext())
      {
         EjbRefMetaData ref = (EjbRefMetaData)enum.next();
         log.debug("Binding an EJBReference "+ref.getName());

         if (ref.getLink() != null)
         {
            // Internal link
            String linkName = ref.getLink();
            String jndiName = EjbUtil.findEjbLink(server, di, linkName);
            log.debug("Binding "+ref.getName()+" to ejb-link: "+linkName+" -> "+jndiName);
            if( jndiName == null )
            {
               String msg = "Failed to resolve ejb-link: "+linkName
                  +" make by ejb-name: "+ref.getName();
               throw new DeploymentException(msg);
            }
            log.debug("Link resolved to:"+jndiName);
            Util.bind(envCtx, ref.getName(), new LinkRef(jndiName));
         }
         else
         {
            // Bind the bean level ejb-ref/jndi-name
            if (ref.getJndiName() == null)
            {
               throw new DeploymentException("ejb-ref " + ref.getName()+
                   ", expected either ejb-link in ejb-jar.xml " +
                   "or jndi-name in jboss.xml");
            }
            log.debug("Binding "+ref.getName()+" to : "+ref.getJndiName());
            Util.bind(envCtx, ref.getName(), new LinkRef(ref.getJndiName()));
         }
      }

      // Bind resource references
      HashMap resRefs = metaData.getResourceReferences();
      enum = resRefs.values().iterator();
      while(enum.hasNext())
      {
         ResourceRefMetaData ref = (ResourceRefMetaData)enum.next();
         String refName = ref.getRefName();
         String jndiName = ref.getJndiName();

         if (ref.getType().equals("java.net.URL"))
         {
            // URL bindings
            log.debug("Binding URL: "+ refName +" to JDNI as: " + jndiName);
            Util.bind(envCtx, refName, new URL(jndiName));
         }
         else
         {
            // A resource link
            log.debug("Binding resource: "+refName+" to JDNI as: " +jndiName);
            Util.bind(envCtx, refName, new LinkRef(jndiName));
         }
      }

      // Bind resource env references
      HashMap envRefs = metaData.getResourceEnvReferences();
      enum = envRefs.values().iterator();
      while( enum.hasNext() )
      {
         ResourceEnvRefMetaData resRef = (ResourceEnvRefMetaData) enum.next();
         String encName = resRef.getRefName();
         String jndiName = resRef.getJndiName();
         // Should validate the type...
         log.debug("Binding env resource: " + encName +
                  " to JDNI as: " +jndiName);
         Util.bind(envCtx, encName, new LinkRef(jndiName));
      }

   }
}
