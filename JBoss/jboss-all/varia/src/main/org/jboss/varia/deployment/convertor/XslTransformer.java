/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.varia.deployment.convertor;

import org.w3c.dom.Node;
import org.w3c.dom.Document;
import org.xml.sax.SAXException;

import java.util.Properties;
import java.util.Enumeration;
import java.util.Map;
import java.util.Iterator;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import javax.xml.transform.TransformerFactory;
import javax.xml.transform.Transformer;
import javax.xml.transform.Templates;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.Source;
import javax.xml.transform.Result;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.dom.DOMResult;

import javax.xml.transform.stream.StreamSource;
import javax.xml.transform.stream.StreamResult;
import javax.xml.parsers.DocumentBuilder;


/**
 * XslTransformer is a utility class for XSL transformations.
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 */
public class XslTransformer
{
   // Attributes --------------------------------------------------------
   /**
    The system property that determines which Factory implementation
    to create is named "javax.xml.transform.TransformerFactory".
    If the property is not defined, a platform default is used.
    An implementation of the TransformerFactory class is NOT guaranteed
    to be thread safe.
    */
   private static TransformerFactory TRANSFORMER_FACTORY = TransformerFactory.newInstance();

   public static Node transform(File xmlFile,
                                String xslPath,
                                Map params,
                                Properties outputProps,
                                Node resultNode)
      throws SAXException, IOException, TransformerException
   {
      DocumentBuilder builder = WebLogicConvertor.newDocumentBuilder();
      if(resultNode == null)
         resultNode = builder.newDocument();
      Document ddDoc = builder.parse(xmlFile);

      InputStream xslIn = Thread.currentThread().getContextClassLoader().getResourceAsStream(xslPath);
      Transformer transformer = TRANSFORMER_FACTORY.newTransformer(new StreamSource(xslIn));
      transformer.setURIResolver(WebLogicConvertor.LocalURIResolver.INSTANCE);
      if(outputProps != null)
         transformer.setOutputProperties(outputProps);

      Iterator paramIter = params.entrySet().iterator();
      while(paramIter.hasNext())
      {
         Map.Entry entry = (Map.Entry) paramIter.next();
         transformer.setParameter((String) entry.getKey(), entry.getValue());
      }

      Result result = new DOMResult(resultNode);
      transformer.transform(new DOMSource(ddDoc), result);

      return resultNode;
   }

   public static void domToFile(Node node, File dest, Properties outputProps)
      throws TransformerException, FileNotFoundException
   {
      Transformer transformer = null;
      try
      {
         transformer = TRANSFORMER_FACTORY.newTransformer();
      }
      catch(TransformerConfigurationException e)
      {
         throw new IllegalStateException("Could not create an instance of " + Transformer.class.getName());
      }

      transformer.setOutputProperties(outputProps);

      Source src = new DOMSource(node);
      OutputStream out = new FileOutputStream(new File(dest, outputProps.getProperty("newname")));
      try
      {
         transformer.transform(src, new StreamResult(out));
      }
      finally
      {
         try
         {
            out.close();
         }
         catch(IOException e)
         {
            throw new IllegalStateException("Could not close file " + dest.getAbsolutePath());
         }
      }
   }

   // Public static methods ---------------------------------------------
   /**
    Applies transformation.
    Pre-compiled stylesheet should be used in a thread-safe manner grabbing
    a new transformer before completing the transformation.
    */
   public static synchronized void applyTransformation(InputStream srcIs,
                                                       OutputStream destOs,
                                                       InputStream templateIs,
                                                       Properties outputProps)
      throws TransformerException
   {
      StreamSource source = new StreamSource(srcIs);
      StreamResult result = new StreamResult(destOs);
      StreamSource template = new StreamSource(templateIs);

      /*
        Pre-compile the stylesheet. This Templates object may be used
        concurrently across multiple threads. Creating a Templates object
        allows the TransformerFactory to do detailed performance optimization
        of transformation instructions, without penalizing runtime
        transformation.
      */
      Templates templates = TRANSFORMER_FACTORY.newTemplates(template);

      Transformer transformer = templates.newTransformer();
      if(outputProps != null)
      {
         transformer.setOutputProperties(outputProps);
      }

      transformer.transform(source, result);
   }


   /**
    * Applies template <code>templateIs</code> to xml source
    * <code>srcIs</code> with output properties <code>outputProps</code>
    * and parameters <code>xslParams</code>.
    * The resulting xml is written to <code>destOs</code>
    */
   public static synchronized void applyTransformation(InputStream srcIs,
                                                       OutputStream destOs,
                                                       InputStream templateIs,
                                                       Properties outputProps,
                                                       Properties xslParams)
      throws TransformerException
   {
      StreamSource source = new StreamSource(srcIs);
      StreamResult result = new StreamResult(destOs);
      StreamSource template = new StreamSource(templateIs);

      Templates templates = TRANSFORMER_FACTORY.newTemplates(template);

      // set output properties
      Transformer transformer = templates.newTransformer();
      if(outputProps != null)
      {
         transformer.setOutputProperties(outputProps);
      }

      // set xsl parameters
      if(xslParams != null)
      {
         // note, xslParams.keys() will not work properly,
         // because it will not return the keys for default properties.
         Enumeration keys = xslParams.propertyNames();
         while(keys.hasMoreElements())
         {
            String key = (String) keys.nextElement();
            transformer.setParameter(key, xslParams.getProperty(key));
         }
      }

      transformer.transform(source, result);
   }

   public static synchronized void applyTransformation(DOMSource srcIs,
                                                       OutputStream destOs,
                                                       InputStream templateIs,
                                                       Properties outputProps,
                                                       Properties xslParams)
      throws TransformerException
   {
      StreamResult result = new StreamResult(destOs);
      StreamSource template = new StreamSource(templateIs);

      Templates templates = TRANSFORMER_FACTORY.newTemplates(template);

      // set output properties
      Transformer transformer = templates.newTransformer();
      if(outputProps != null)
      {
         transformer.setOutputProperties(outputProps);
      }

      // set xsl parameters
      if(xslParams != null)
      {
         // note, xslParams.keys() will not work properly,
         // because it will not return the keys for default properties.
         Enumeration keys = xslParams.propertyNames();
         while(keys.hasMoreElements())
         {
            String key = (String) keys.nextElement();
            transformer.setParameter(key, xslParams.getProperty(key));
         }
      }

      transformer.setURIResolver(WebLogicConvertor.LocalURIResolver.INSTANCE);
      transformer.transform(srcIs, result);
   }
}
