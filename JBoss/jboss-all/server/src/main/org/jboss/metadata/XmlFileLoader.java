/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org
 */
package org.jboss.metadata;

import java.net.URL;
import java.io.IOException;
import java.io.InputStream;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;

import org.w3c.dom.Document;
import org.xml.sax.ErrorHandler;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.InputSource;

import org.jboss.deployment.DeploymentException;
import org.jboss.deployment.JBossEntityResolver;
import org.jboss.logging.Logger;

/** XmlFileLoader class is used to read ejb-jar.xml, standardjboss.xml, jboss.xml
 * files, process them using DTDs and create ApplicationMetaData object for
 * future use. It also provides the local entity resolver for the JBoss
 * specific DTDs.
 *
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:WolfgangWerner@gmx.net">Wolfgang Werner</a>
 * @author <a href="mailto:Darius.D@jbees.com">Darius Davidavicius</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @version $Revision: 1.28.4.8 $
 */
public class XmlFileLoader
{
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   private static boolean defaultValidateDTDs = false;
   private static Logger log = Logger.getLogger(XmlFileLoader.class);
   private ClassLoader classLoader;
   private ApplicationMetaData metaData;
   private boolean validateDTDs;
   
   // Static --------------------------------------------------------
   public static boolean getDefaultValidateDTDs()
   {
      return defaultValidateDTDs;
   }
   
   public static void setDefaultValidateDTDs(boolean validate)
   {
      defaultValidateDTDs = validate;
   }
   
   
   // Constructors --------------------------------------------------
   public XmlFileLoader()
   {
      this(defaultValidateDTDs);
   }
   
   public XmlFileLoader(boolean validateDTDs)
   {
      this.validateDTDs = validateDTDs;
   }
   
   // Public --------------------------------------------------------
   public ApplicationMetaData getMetaData()
   {
      return metaData;
   }
   
   /**
    * Set the class loader
    *
    * @param ClassLoader cl - class loader
    */
   public void setClassLoader(ClassLoader cl)
   {
      classLoader = cl;
   }
   
   /**
    * Gets the class loader
    *
    * @return ClassLoader - the class loader
    */
   public ClassLoader getClassLoader()
   {
      return classLoader;
   }
   
   
   /** Get the flag indicating that ejb-jar.dtd, jboss.dtd &
    * jboss-web.dtd conforming documents should be validated
    * against the DTD.
    */
   public boolean getValidateDTDs()
   {
      return validateDTDs;
   }
   
   /** Set the flag indicating that ejb-jar.dtd, jboss.dtd &
    * jboss-web.dtd conforming documents should be validated
    * against the DTD.
    */
   public void setValidateDTDs(boolean validate)
   {
      this.validateDTDs = validate;
   }
   
   /**
    * load()
    *
    * This method creates the ApplicationMetaData.
    * The configuration files are found in the classLoader.
    * The default jboss.xml and jaws.xml files are always read first, then we override
    * the defaults if the user provides them
    */
   public ApplicationMetaData load() throws Exception
   {
      // create the metadata
      metaData = new ApplicationMetaData();
      // Load ejb-jar.xml
      // we can always find the files in the classloader
      URL ejbjarUrl = getClassLoader().getResource("META-INF/ejb-jar.xml");
      if (ejbjarUrl == null)
      {
         throw new DeploymentException("no ejb-jar.xml found");
      }
      
      Document ejbjarDocument = getDocumentFromURL(ejbjarUrl);
      
      // the url may be used to report errors
      metaData.setUrl(ejbjarUrl);
      metaData.importEjbJarXml(ejbjarDocument.getDocumentElement());
      
      // Load jbossdefault.xml from the default classLoader
      // we always load defaults first
      // we use the context classloader, because this guy has to know where
      // this file is
      URL defaultJbossUrl = Thread.currentThread().getContextClassLoader().getResource("standardjboss.xml");
      if (defaultJbossUrl == null)
      {
         throw new DeploymentException("no standardjboss.xml found");
      }

      Document defaultJbossDocument = null;
      try
      {
         defaultJbossDocument = getDocumentFromURL(defaultJbossUrl);
         metaData.setUrl(defaultJbossUrl);
         metaData.importJbossXml(defaultJbossDocument.getDocumentElement());
      }
      catch (Exception ex)
      {
         log.error("failed to load standardjboss.xml.  There could be a syntax error.", ex);
         throw ex;
      }
      
      // Load jboss.xml
      // if this file is provided, then we override the defaults
      try
      {
         URL jbossUrl = getClassLoader().getResource("META-INF/jboss.xml");
         if (jbossUrl != null)
         {
            Document jbossDocument = getDocumentFromURL(jbossUrl);
            metaData.setUrl(jbossUrl);
            metaData.importJbossXml(jbossDocument.getDocumentElement());
         }
      }
      catch (Exception ex)
      {
         log.error("failed to load jboss.xml.  There could be a syntax error.", ex);
         throw ex;
      }
      
      return metaData;
   }

   /** Invokes getDocument(url, defaultValidateDTDs)
    *
    */
   public static Document getDocument(URL url) throws DeploymentException
   {
      return getDocument(url, defaultValidateDTDs);
   }
   
   /** Get the xml file from the URL and parse it into a Document object.
    * Calls new XmlFileLoader(validateDTDs).getDocumentFromURL(url);
    * @param url, the URL from which the xml doc is to be obtained.
    * @return Document
    */
   public static Document getDocument(URL url, boolean validateDTDs) throws DeploymentException
   {
      XmlFileLoader loader = new XmlFileLoader(validateDTDs);
      return loader.getDocumentFromURL(url);
   }
   
   /** Get the xml file from the URL and parse it into a Document object.
    * Calls getDocument(new InputSource(url.openStream()), url.getPath())
    * with the InputSource.SystemId set to url.toExternalForm().
    *
    * @param url, the URL from which the xml doc is to be obtained.
    * @return Document
    */
   public Document getDocumentFromURL(URL url) throws DeploymentException
   {
      InputStream is = null;
      try
      {
         is = url.openStream();
         return getDocument(is, url.toExternalForm());
      }
      catch (IOException e)
      {
         throw new DeploymentException("Failed to obtain xml doc from URL", e);
      }
   }

   /** Parses the xml document in is to create a DOM Document. DTD validation
    * is enabled if validateDTDs is true and we install an EntityResolver and
    * ErrorHandler to resolve J2EE DTDs and handle errors. We also create an
    * InputSource for the InputStream and set the SystemId URI to the inPath
    * value. This allows relative entity references to be resolved against the
    * inPath URI. The is argument will be closed.
    *
    * @param is, the InputStream containing the xml descriptor to parse
    * @param inPath, the path information for the xml doc. This is used as the
    * InputSource SystemId URI for resolving relative entity references.
    * @return Document
    */
   public Document getDocument(InputStream is, String inPath)
      throws DeploymentException
   {
      InputSource is2 = new InputSource(is);
      is2.setSystemId(inPath);
      Document doc = null;
      try
      {
         doc = getDocument(is2, inPath);
      }
      finally
      {
         // close the InputStream to get around "too many open files" errors
         // with large heaps
         try
         {
            if( is != null )
              is.close();
         }
         catch (Exception e)
         {
            // ignore
         }
      }
      return doc;
   }

   /** Parses the xml document in is to create a DOM Document. DTD validation
    * is enabled if validateDTDs is true and we install an EntityResolver and
    * ErrorHandler to resolve J2EE DTDs and handle errors. We also create an
    * InputSource for the InputStream and set the SystemId URI to the inPath
    * value. This allows relative entity references to be resolved against the
    * inPath URI.
    *
    * @param is, the InputSource containing the xml descriptor to parse
    * @param inPath, the path information for the xml doc. This is used for
    * only for error reporting.
    * @return Document
    */
   public Document getDocument(InputSource is, String inPath)
      throws DeploymentException
   {
      try
      {
         DocumentBuilderFactory docBuilderFactory = DocumentBuilderFactory.newInstance();

         // Enable DTD validation based on our validateDTDs flag
         docBuilderFactory.setValidating(validateDTDs);
         DocumentBuilder docBuilder = docBuilderFactory.newDocumentBuilder();
         JBossEntityResolver lr = new JBossEntityResolver();
         LocalErrorHandler eh = new LocalErrorHandler( inPath, lr );
         docBuilder.setEntityResolver(lr);
         docBuilder.setErrorHandler(eh );

         Document doc = docBuilder.parse(is);
         if(validateDTDs && eh.hadError())
         {
            throw new DeploymentException("Invalid XML: file=" + inPath);
         }
         return doc;
      }
      catch (DeploymentException e) 
      {
         throw e;
      }
      catch (SAXParseException e)
      {
         log.error(e.getMessage()+":"+e.getColumnNumber()+":"+e.getLineNumber(), e);
         throw new DeploymentException(e.getMessage(), e);
      }
      catch (SAXException e)
      {
         System.out.println(e.getException());
         throw new DeploymentException(e.getMessage(), e);
      }
      catch (Exception e)
      {
         throw new DeploymentException(e.getMessage(), e);
      }
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

   /** Local error handler for entity resolver to DocumentBuilder parser.
    * Error is printed to output just if DTD was detected in the XML file.
    * If DTD was not found in XML file it is assumed that the EJB builder
    * doesn't want to use DTD validation. Validation may have been enabled via
    * validateDTDs flag so we look to the hasDTD() function in the LocalResolver
    * and reject errors if DTD not used.
    **/
   private static class LocalErrorHandler implements ErrorHandler
   {
      // The xml file being parsed
      private String theFileName;
      private JBossEntityResolver localResolver;
      private boolean error;
      
      public LocalErrorHandler( String inFileName, JBossEntityResolver localResolver )
      {
         this.theFileName = inFileName;
         this.localResolver = localResolver;
         this.error = false;
      }
      
      public void error(SAXParseException exception)
      {
         if ( localResolver.hasDTD() )
         {
            this.error = true;
            log.error("XmlFileLoader: File "
            + theFileName
            + " process error. Line: "
            + String.valueOf(exception.getLineNumber())
            + ". Error message: "
            + exception.getMessage()
            );
         }//end if
      }

      public void fatalError(SAXParseException exception)
      {
         if ( localResolver.hasDTD() )
         {
            this.error = true;
            log.error("XmlFileLoader: File "
            + theFileName
            + " process fatal error. Line: "
            + String.valueOf(exception.getLineNumber())
            + ". Error message: "
            + exception.getMessage()
            );
         }//end if
      }
      
      public void warning(SAXParseException exception)
      {
         if ( localResolver.hasDTD() )
         {
            this.error = true;
            log.error("XmlFileLoader: File "
            + theFileName
            + " process warning. Line: "
            + String.valueOf(exception.getLineNumber())
            + ". Error message: "
            + exception.getMessage()
            );
         }//end if
      }

      public boolean hadError() {
         return error;
      }
   }// end class LocalErrorHandler
}
