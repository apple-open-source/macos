package org.jboss.deployment;

import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.io.UnsupportedEncodingException;

import javax.management.ObjectName;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.Source;
import javax.xml.transform.Templates;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMResult;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamSource;
import javax.xml.transform.stream.StreamResult;

import org.jboss.mx.util.MBeanProxy;
import org.jboss.util.xml.DOMWriter;
import org.w3c.dom.Document;
import org.xml.sax.SAXException;

/**
 * XSLSubDeployer.java
 *
 *
 * Created: Fri Jul 12 09:54:51 2002
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 * @jmx.mbean name="jboss.system:service=XSLDeployer"
 *            extends="org.jboss.deployment.SubDeployerMBean"
 */

public class XSLSubDeployer 
   extends SubDeployerSupport
   implements XSLSubDeployerMBean
{

   protected String xslUrl;

   protected String packageSuffix;

   protected String ddSuffix;

   protected DocumentBuilderFactory dbf;

   private Templates templates;

   protected ObjectName delegateName = SARDeployerMBean.OBJECT_NAME;

   protected SubDeployer delegate;

   public XSLSubDeployer (){
      
   }

   /**
    * Describe <code>setXslUrl</code> method here.
    *
    * @param xslUrl a <code>String</code> value
    *
    * @jmx.managed-attribute
    */
   public void setXslUrl(final String xslUrl)
   {
      this.xslUrl = xslUrl;
   }

   /**
    * Describe <code>getXslUrl</code> method here.
    *
    * @return a <code>String</code> value
    *
    * @jmx.managed-attribute
    */
   public String getXslUrl()
   {
      return xslUrl;
   }

   /**
    * Describe <code>setPackageSuffix</code> method here.
    *
    * @param packageSuffix a <code>String</code> value
    *
    * @jmx.managed-attribute
    */
   public void setPackageSuffix(final String packageSuffix)
   {
      this.packageSuffix = packageSuffix;
   }

   /**
    * Describe <code>getPackageSuffix</code> method here.
    *
    * @return a <code>String</code> value
    *
    * @jmx.managed-attribute
    */
   public String getPackageSuffix()
   {
      return packageSuffix;
   }

   /**
    * Describe <code>setDdSuffix</code> method here.
    *
    * @param ddSuffix a <code>String</code> value
    *
    * @jmx.managed-attribute
    */
   public void setDdSuffix(final String ddSuffix)
   {
      this.ddSuffix = ddSuffix;
   }

   /**
    * Describe <code>getDdSuffix</code> method here.
    *
    * @return a <code>String</code> value
    *
    * @jmx.managed-attribute
    */
   public String getDdSuffix()
   {
      return ddSuffix;
   }

   /**
    * Describe <code>setDelegateName</code> method here.
    *
    * @param delegateName an <code>ObjectName</code> value
    *
    * @jmx.managed-attribute
    */
   public void setDelegateName(final ObjectName delegateName)
   {
      this.delegateName = delegateName;
   }

   /**
    * Describe <code>getDelegateName</code> method here.
    *
    *
    * @jmx.managed-attribute
    */
   public ObjectName getDelegateName()
   {
      return delegateName;
   }

   protected void createService() throws Exception
   {
      super.createService();
      delegate = (SubDeployer)MBeanProxy.get(SubDeployer.class, delegateName, server);
      TransformerFactory tf = TransformerFactory.newInstance();

      dbf = DocumentBuilderFactory.newInstance();
      dbf.setNamespaceAware(true);
      InputStream is = Thread.currentThread().getContextClassLoader().getResourceAsStream(xslUrl);
      StreamSource ss = new StreamSource(is);
      templates = tf.newTemplates(ss);
      log.info("Created templates: " + templates);
   }

   protected void destroyService() throws Exception
   {
      templates = null;
      super.destroyService();
   }

   public boolean accepts(DeploymentInfo di)
   {
      String urlStr = di.url.toString();
      return (packageSuffix != null
	      && (urlStr.endsWith(packageSuffix) 
		  || urlStr.endsWith(packageSuffix + "/"))) 
	 || (ddSuffix != null 
	     && urlStr.endsWith(ddSuffix));
   }

   public void init(DeploymentInfo di) throws DeploymentException
   {
      if (di.document == null)
      {
         findDd(di);
      }
      try
      {
         Transformer trans = templates.newTransformer();
         Source s = new DOMSource(di.document);
         DOMResult r = new DOMResult();
         setParameters(trans);
         trans.transform(s, r);
         di.document = (Document) r.getNode();
         if (log.isDebugEnabled())
         {
            StringWriter sw = new StringWriter();
            DOMWriter w = new DOMWriter(sw, false);
            w.print(di.document, true);
            log.debug("transformed into doc: " + sw.getBuffer().toString());
         }
      }
      catch (TransformerException ce)
      {
         throw new DeploymentException("Problem with xsl transformation", ce);
      }
      //super.init(di);
      delegate.init(di);
   }

   public void create(DeploymentInfo di) throws DeploymentException 
   {
      delegate.create(di);
   }
   
   public void start(DeploymentInfo di) throws DeploymentException 
   {
      delegate.start(di);
   }

   public void stop(DeploymentInfo di) throws DeploymentException 
   {
      delegate.stop(di);
   }

   public void destroy(DeploymentInfo di) throws DeploymentException 
   {
      delegate.destroy(di);
   }
   
   protected void setParameters(Transformer trans) throws TransformerException
   {
      //override to set document names etc.
      trans.setParameter("jboss.server.data.dir", System.getProperty("boss.server.data.dir"));
   }

   protected void findDd(DeploymentInfo di) throws DeploymentException
   {

      try
      {
	 DocumentBuilder db = dbf.newDocumentBuilder();
	 String urlStr = di.url.toString();
	 if (packageSuffix != null)
	 {
	    if (urlStr.endsWith(packageSuffix))
	    {
	    }
	    else if (urlStr.endsWith(packageSuffix + "/"))
	    {
	    }
	 } 
	 if (ddSuffix != null 
	     && urlStr.endsWith(ddSuffix))
	 {
	    di.document = (Document)db.parse(urlStr);
	 }
      }
      catch (SAXException se)
      {
	 throw new DeploymentException("Could not parse dd", se);
      }
      catch (IOException ioe)
      {
	 throw new DeploymentException("Could not read dd", ioe);
      }
      catch (ParserConfigurationException pce)
      {
	 throw new DeploymentException("Could not create document builder for dd", pce);
      }
   }
}// XSLSubDeployer
