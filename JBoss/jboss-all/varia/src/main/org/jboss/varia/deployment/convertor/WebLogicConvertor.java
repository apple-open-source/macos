/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.varia.deployment.convertor;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.util.Properties;
import java.util.Map;
import java.util.HashMap;
import java.util.Collections;
import java.util.jar.JarFile;
import java.net.URL;

import javax.management.JMException;
import javax.management.ObjectName;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.URIResolver;
import javax.xml.transform.Source;
import javax.xml.transform.TransformerException;
import javax.xml.transform.dom.DOMSource;

import org.jboss.deployment.DeploymentInfo;
import org.jboss.system.ServiceMBeanSupport;
import org.w3c.dom.Document;
import org.w3c.dom.DocumentType;
import org.w3c.dom.Element;
import org.w3c.dom.NodeList;
import org.w3c.dom.Node;
import org.xml.sax.InputSource;
import org.xml.sax.EntityResolver;
import org.xml.sax.SAXException;


/**
 * Converts WebLogic applications.
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.4.2.4 $
 *
 * <p><b>20020519 Andreas Schaefer:</b>
 * <ul>
 *    <li>Creation</li>
 * </ul>
 *
 * @jmx.mbean
 *    name="jboss.system:service=Convertor,type=WebLogic"
 *    extends="org.jboss.system.ServiceMBean"
 */
public class WebLogicConvertor
   extends ServiceMBeanSupport
   implements Convertor, WebLogicConvertorMBean
{
   // Static
   private static final String WEBLOGIC_EJB_JAR_510 = "-//BEA Systems, Inc.//DTD WebLogic 5.1.0 EJB//EN";

   private static final DocumentBuilderFactory DOC_BUILDER_FACTORY = DocumentBuilderFactory.newInstance();

   public static DocumentBuilder newDocumentBuilder()
   {
      DocumentBuilder builder = null;
      try
      {
         builder = DOC_BUILDER_FACTORY.newDocumentBuilder();
      }
      catch(ParserConfigurationException e)
      {
         throw new IllegalStateException("Could not create an instance of " + DocumentBuilder.class.getName());
      }
      builder.setEntityResolver(LocalEntityResolver.INSTANCE);
      return builder;
   }

   // Attributes ---------------------------------------
   /** the deployer name this converter is registered with */
   private String deployerName;

   /** the version of xsl resources to apply */
   private String wlVersion;

   /** remove-table value */
   private String removeTable;

   /** datasource name that will be set up for converted bean */
   private String datasource;

   /** the datasource mapping for the datasource */
   private String datasourceMapping;

   /** xsl parameters used in transformations */
   private Properties xslParams;

   // WebLogicConverter implementation -----------------
   /**
    * @jmx.managed-attribute
    */
   public String getDeployer()
   {
      return deployerName;
   }

   /**
    * @jmx.managed-attribute
    */
   public void setDeployer(String name)
   {
      if(deployerName != null && name != null && deployerName != name)
      {
         // Remove deployer
         try
         {
            server.invoke(
               new ObjectName(deployerName),
               "removeConvertor",
               new Object[]{this},
               new String[]{this.getClass().getName()}
            );
         }
         catch(JMException jme)
         {
         }
      }
      if(name != null) deployerName = name;
   }

   /**
    * @jmx.managed-attribute
    */
   public String getWlVersion()
   {
      return wlVersion;
   }

   /**
    * @jmx.managed-attribute
    */
   public void setWlVersion(String wlVersion)
   {
      this.wlVersion = wlVersion;
   }

   /**
    * @jmx.managed-attribute
    */
   public String getRemoveTable()
   {
      return removeTable;
   }

   /**
    * @jmx.managed-attribute
    */
   public void setRemoveTable(String removeTable)
   {
      this.removeTable = removeTable;
   }

   /**
    * @jmx.managed-attribute
    */
   public String getDatasource()
   {
      return datasource;
   }

   /**
    * @jmx.managed-attribute
    */
   public void setDatasource(String datasource)
   {
      this.datasource = datasource;
   }

   /**
    * @jmx.managed-attribute
    */
   public String getDatasourceMapping()
   {
      return datasourceMapping;
   }

   /**
    * @jmx.managed-attribute
    */
   public void setDatasourceMapping(String datasourceMapping)
   {
      this.datasourceMapping = datasourceMapping;
   }

   // ServiceMBeanSupport overridding ------------------
   public void startService()
   {
      try
      {
         // init xsl params first
         initXslParams();

         server.invoke(
            new ObjectName(deployerName),
            "addConvertor",
            new Object[]{this},
            new String[]{Convertor.class.getName()}
         );
      }
      catch(JMException jme)
      {
         log.error("Caught exception during startService()", jme);
      }
   }

   public void stopService()
   {
      if(deployerName != null)
      {
         // Remove deployer
         try
         {
            server.invoke(
               new ObjectName(deployerName),
               "removeConvertor",
               new Object[]{this},
               new String[]{this.getClass().getName()}
            );
         }
         catch(JMException jme)
         {
            // Ingore
         }
      }
   }

   // Converter implementation ----------------------------------------
   /**
    * Checks if the deployment can be converted to a JBoss deployment
    * by this converter.
    *
    * @param url The url of deployment to be converted
    * @return true if this converter is able to convert
    */
   public boolean accepts(URL url)
   {
      String stringUrl = url.toString();
      JarFile jarFile = null;
      boolean accepted = false;
      try
      {
         jarFile = new JarFile(url.getPath());
         accepted = (jarFile.getEntry("META-INF/weblogic-ejb-jar.xml") != null)
            && (stringUrl.endsWith(".wlar") || (stringUrl.endsWith(".wl")))
            || stringUrl.endsWith(".war.wl")
            || stringUrl.endsWith(".ear.wl");
         jarFile.close();
      }
      catch(Exception e)
      {
         log.debug("Couldn't create JarFile for " + url.getPath(), e);
         return false;
      }

      return accepted;
   }

   /**
    * Converts the necessary files to make the given deployment deployable
    * on JBoss
    *
    * @param di The deployment to be converted
    * @param path Path of the extracted deployment
    **/
   public void convert(DeploymentInfo di, File path)
      throws Exception
   {
      Properties xslParams = getXslParams();

      File weblogicEjbJar = new File(path, "META-INF/weblogic-ejb-jar.xml");
      if(!weblogicEjbJar.exists())
      {
         JarTransformer.transform(path, xslParams);
      }
      else
      {
         Document doc = newDocumentBuilder().parse(weblogicEjbJar);
         DocumentType doctype = doc.getDoctype();
         String publicId = doctype.getPublicId();

         if(WEBLOGIC_EJB_JAR_510.equals(publicId))
         {
            log.debug("weblogic 5.1.0 application.");
            new WLS51EJBConverter().convert(path);
         }
         else
         {
            JarTransformer.transform(path, xslParams);
         }
      }
   }

   // Public -------------------------------------------
   /**
    * Returns the XSL parameters
    */
   public Properties getXslParams()
   {
      if(xslParams == null)
      {
         log.warn("xmlParams should have been initialized!");
         xslParams = initXslParams();
      }

      // xsl resources path
      xslParams.setProperty("resources_path", "resources/" + wlVersion + "/");

      // set remove-table
      xslParams.setProperty("remove-table", removeTable);

      // datasource
      xslParams.setProperty("datasource", datasource);

      // datasource-mapping
      xslParams.setProperty("datasource-mapping", datasourceMapping);

      return xslParams;
   }

   // Private -------------------------------------------------------
   /**
    * Initializes XSL parameters
    */
   private Properties initXslParams()
   {
      xslParams = new Properties();

      ClassLoader cl = Thread.currentThread().getContextClassLoader();

      // path to standardjboss.xml
      URL url = cl.getResource("standardjboss.xml");
      if(url != null)
         xslParams.setProperty("standardjboss",
            new File(url.getFile()).getAbsolutePath());
      else
         log.debug("standardjboss.xml not found.");

      // path to standardjbosscmp-jdbc.xml
      url = cl.getResource("standardjbosscmp-jdbc.xml");
      if(url != null)
         xslParams.setProperty("standardjbosscmp-jdbc",
            new File(url.getFile()).getAbsolutePath());
      else
         log.debug("standardjbosscmp-jdbc.xml not found.");

      log.debug("initialized xsl parameters: " + xslParams);

      return xslParams;
   }

   // Inner

   private final class WLS51EJBConverter
   {
      public void convert(File root)
         throws Exception
      {
         // ejb-jar.xml
         File ejbJarXml = getDDFile(root, "ejb-jar.xml", true);

         // weblogic-ejb-jar.xml
         File weblogicEjbJarXml = getDDFile(root, "weblogic-ejb-jar.xml", true);

         // weblogic-ejb-jar-output.properties
         Properties outputProps = loadProperties("resources/5.1/weblogic-ejb-jar-output.properties");

         DocumentBuilder builder = newDocumentBuilder();

         //
         // weblogic-ejb-jar.xml
         //
         Node result = XslTransformer.transform(
            weblogicEjbJarXml,
            "resources/5.1/weblogic-ejb-jar.xsl",
            Collections.singletonMap("ejb-jar-xml", ejbJarXml.getAbsolutePath()),
            outputProps,
            null
         );
         XslTransformer.domToFile(result, root, outputProps);

         //
         // cmp-rdbms DDs
         //
         Document doc = builder.newDocument();

         Element jbosscmpjdbc = doc.createElement("jbosscmp-jdbc");
         doc.appendChild(jbosscmpjdbc);
         Element enterpriseBeans = doc.createElement("enterprise-beans");
         jbosscmpjdbc.appendChild(enterpriseBeans);

         Document ejbJarDoc = builder.parse(ejbJarXml);
         NodeList entityList = ejbJarDoc.getElementsByTagName("entity");
         if(entityList.getLength() == 0)
         {
            log.error("NO ENTITIES IN EJB-JAR.XML!!!");
         }

         Map params = new HashMap(2);
         params.put("ejb-jar-xml", ejbJarXml.getAbsolutePath());

         File weblogicCmpRdbmsJarXml = getDDFile(root, "weblogic-cmp-rdbms-jar.xml", false);
         if(weblogicCmpRdbmsJarXml.exists())
         {
            log.debug("Transforming " + weblogicCmpRdbmsJarXml);

            Node entity = entityList.item(0);
            String remote = getRemote(entity);
            params.put("remote", remote);

            log.debug("remote: " + remote);

            XslTransformer.transform(
               weblogicCmpRdbmsJarXml,
               "resources/5.1/weblogic-cmp-rdbms-jar.xsl",
               params,
               null,
               enterpriseBeans
            );
         }
         else
         {
            // no weblogic-cmp-rdbms-jar.xml -> look for files <remote-interface>-cmp-rdbms.xml
            for(int entityInd = 0; entityInd < entityList.getLength(); ++entityInd)
            {
               Node entity = entityList.item(entityInd);
               String remote = getRemote(entity);
               int lastDot = remote.lastIndexOf('.');
               String shortRemote = (lastDot > -1 ? remote.substring(lastDot + 1) : remote);

               String ddName = shortRemote + "-cmp-rdbms.xml";
               File ddFile = getDDFile(root, ddName, false);
               if(ddFile.exists())
               {
                  log.debug("processing " + ddFile.getName());
                  params.put("remote", remote);
                  XslTransformer.transform(
                     ddFile,
                     "resources/5.1/weblogic-cmp-rdbms-jar.xsl",
                     params,
                     null,
                     enterpriseBeans
                  );
               }
               else
               {
                  log.warn("NO " + ddName + " was found.");
               }
            }
         }

         outputProps = loadProperties("resources/5.1/weblogic-cmp-rdbms-jar-output.properties");
         XslTransformer.domToFile(doc, root, outputProps);
      }

      private String getRemote(Node entity)
      {
         String result = null;
         NodeList entityContent = entity.getChildNodes();
         for(int i = 0; i < entityContent.getLength(); ++i)
         {
            Node item = entityContent.item(i);
            if("remote".equals(item.getNodeName()))
            {
               result = item.getFirstChild().getNodeValue();
               break;
            }
         }

         if(result == null)
         {
            log.error("REMOTE INTERFACE NOT FOUND IN EJB-JAR.XML");
         }

         return result;
      }

      private Properties loadProperties(String propsPath)
      {
         ClassLoader cl = Thread.currentThread().getContextClassLoader();
         InputStream propsIn = cl.getResourceAsStream(propsPath);
         Properties props = new Properties();
         if(propsIn != null)
         {
            try
            {
               props.load(propsIn);
            }
            catch(IOException e)
            {
               log.warn("Could not load output properties: " + propsPath);
            }
         }
         else
         {
            log.warn("Could not find output properties: " + propsPath);
         }
         return props;
      }

      // Private

      private File getDDFile(File parent, String ddName, boolean failIfNotFound)
      {
         File ddFile = new File(parent, "META-INF/" + ddName);
         if(failIfNotFound && !ddFile.exists())
            throw new IllegalStateException("META-INF/" + ddName + " not found!");
         return ddFile;
      }
   }

   private static class LocalEntityResolver implements EntityResolver
   {
      private static final String WEBLOGIC_EJB_JAR_600 =
         "-//BEA Systems, Inc.//DTD WebLogic 6.0.0 EJB//EN";
      private static final String WEBLOGIC_RDBMS_PERSISTENCE_600 =
         "-//BEA Systems, Inc.//DTD WebLogic 6.0.0 EJB RDBMS Persistence//EN";
      private static final String WEBLOGIC_RDBMS_PERSISTENCE_510 =
         "-//BEA Systems, Inc.//DTD WebLogic 5.1.0 EJB RDBMS Persistence//EN";
      private static final String WEBLOGIC_EJB_JAR_510 =
         "-//BEA Systems, Inc.//DTD WebLogic 5.1.0 EJB//EN";
      private static final String SUN_EJB_20=
         "-//Sun Microsystems, Inc.//DTD Enterprise JavaBeans 2.0//EN";
      private static final String SUN_EJB_11 =
         "-//Sun Microsystems, Inc.//DTD Enterprise JavaBeans 1.1//EN";

      private static final Map DTD_BY_ID = new HashMap(3);
      static
      {
         String base51 = "resources/5.1/";
         DTD_BY_ID.put(WEBLOGIC_RDBMS_PERSISTENCE_510, base51 + "weblogic-rdbms-persistence.dtd");
         DTD_BY_ID.put(WEBLOGIC_EJB_JAR_510, base51 + "weblogic-ejb-jar.dtd");
         DTD_BY_ID.put(SUN_EJB_11, base51 + "ejb-jar.dtd");

         String base61 = "resources/6.1/";
         DTD_BY_ID.put(WEBLOGIC_RDBMS_PERSISTENCE_600, base61 + "weblogic-rdbms20-persistence-600.dtd");
         DTD_BY_ID.put(WEBLOGIC_EJB_JAR_600, base61 + "weblogic-ejb-jar.dtd");
         DTD_BY_ID.put(SUN_EJB_20, base61 + "ejb-jar_2_0.dtd");
      }

      public static final EntityResolver INSTANCE = new LocalEntityResolver();

      // Constructor

      private LocalEntityResolver()
      {
      }

      public InputSource resolveEntity(String publicId,
                                       String systemId)
         throws SAXException, IOException
      {
         String dtd = (String)DTD_BY_ID.get(publicId);
         if(dtd == null)
         {
            throw new IllegalStateException("Unsupported public id: " + publicId);
         }
         InputStream dtdIn = Thread.currentThread().getContextClassLoader()
            .getResourceAsStream(dtd);
         return new InputSource(dtdIn);
      }
   }

   public static class LocalURIResolver
      implements URIResolver
   {
      public static final URIResolver INSTANCE = new LocalURIResolver();

      private LocalURIResolver()
      {
      }

      public Source resolve(String href, String base)
         throws TransformerException
      {
         DocumentBuilderFactory factory = DocumentBuilderFactory.newInstance();
         DocumentBuilder builder = null;
         try
         {
            builder = factory.newDocumentBuilder();
         }
         catch(ParserConfigurationException e)
         {
            throw new TransformerException("Could not create DocumentBuilder", e);
         }
         builder.setEntityResolver(LocalEntityResolver.INSTANCE);

         Document doc;
         try
         {
            doc = builder.parse(new File(href));
         }
         catch(Exception e)
         {
            throw new TransformerException("Could not parse " + href, e);
         }

         return new DOMSource(doc);
      }
   }
}
