/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.boot.servlets;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.util.Enumeration;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.ServletOutputStream;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.Source;
import javax.xml.transform.stream.StreamSource;
import javax.xml.transform.stream.StreamResult;

import gnu.regexp.RE;
import gnu.regexp.REException;

import org.jboss.logging.Logger;

/** A prototype of a boot installation servlet that can be used as the
class loader for a JBoss server net install. This servlet transforms any
xml content requested using the default.xsl document available under the
transformBaseDir init-param. A configBaseDir init-param that defines the
root of the JBoss server distribution must be defined.

 * @author  Scott.Stark@jboss.org
 * @version $revision:$
 */
public class BootServlet extends HttpServlet
{
   static Logger log = Logger.getLogger(BootServlet.class);
   static File configBase;
   static File transformBase;
   static RE variableRE;
   static TransformerFactory xsltFactory;

   /** Initializes the servlet.
    */
   public void init(ServletConfig config) throws ServletException
   {
      super.init(config);
      String configBaseDir = config.getInitParameter("configBaseDir");
      if( configBaseDir == null )
         throw new ServletException("No configBaseDir parameter specified");
      configBase = new File(configBaseDir);
      if( configBase.exists() == false || configBase.isDirectory() == false )
         throw new ServletException("The configBaseDir("+configBaseDir+") does not exist is not a directory");
      log.info("Using configBaseDir = "+configBase.getAbsolutePath());

      String transformBaseDir = config.getInitParameter("transformBaseDir");
      if( transformBaseDir == null )
         log.warn("No transformBaseDir parameter specified, using classpath resources");
      else
      {
         transformBase = new File(transformBaseDir);
         if( transformBase.exists() == false || transformBase.isDirectory() == false )
            throw new ServletException("The transformBaseDir("+transformBaseDir+") does not exist is not a directory");
         log.info("Using transformBaseDir = "+transformBase.getAbsolutePath());
      }

      String reExp = config.getInitParameter("variableRE");
      if( reExp == null )
         reExp = "([^$]+)?\\${([^}]+)}([^$]+)?";
      try
      {
         variableRE = new RE(reExp);
         Util.setVariableRE(variableRE);
         log.info("Using variableRE = "+variableRE.toString());
      }
      catch(REException e)
      {
         throw new ServletException("Failed to init variableRE", e);
      }

      xsltFactory = TransformerFactory.newInstance();
   }

   /** Processes requests for both HTTP <code>GET</code> and <code>POST</code> methods.
    * @param request servlet request
    * @param response servlet response
    */
   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException, TransformerException
   {
      String pathInfo = request.getPathInfo();
      log.trace("Begin, pathInfo = "+pathInfo);
      if( pathInfo.charAt(0) == '/' )
         pathInfo = pathInfo.substring(1);
      ServletOutputStream out = response.getOutputStream();
      if( isJar(pathInfo) )
         transformJarFile(out, pathInfo);
      else if( isXml(pathInfo) )
         transformFile(out, pathInfo);
      else
         copy(out, pathInfo);
      log.trace("End, pathInfo = "+pathInfo);
   }

   /** Handles the HTTP <code>GET</code> method.
    * @param request servlet request
    * @param response servlet response
    */
   protected void doGet(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      try
      {
         processRequest(request, response);
      }
      catch(TransformerException e)
      {
         throw new ServletException("XLST failure", e);
      }
   }

   /** Handles the HTTP <code>POST</code> method.
    * @param request servlet request
    * @param response servlet response
    */
   protected void doPost(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      try
      {
         processRequest(request, response);
      }
      catch(TransformerException e)
      {
         throw new ServletException("XLST failure", e);
      }
   }

   public String getServletInfo()
   {
      return "JBoss net boot servlet";
   }

   static boolean isJar(String path)
   {
      boolean isJar = path.endsWith(".jar")
         | path.endsWith(".war")
         | path.endsWith(".ear")
         | path.endsWith(".rar")
         | path.endsWith(".sar");
      return isJar;
   }
   static boolean isXml(String path)
   {
      return path.endsWith(".xml");
   }

   static void copy(ServletOutputStream out, String path) throws IOException
   {
      File content = new File(configBase, path);
      FileInputStream fis = new FileInputStream(content);
      BufferedInputStream bis = new BufferedInputStream(fis);
      byte[] buffer = new byte[2048];
      int bytes = bis.read(buffer);
      while( bytes > 0 )
      {
         out.write(buffer, 0, bytes);
         bytes = bis.read(buffer);
      }
      bis.close();
      out.close();
   }
   static void transformFile(ServletOutputStream out, String path)
      throws IOException, TransformerException
   {
      // Get the XML input document and the stylesheet.
      File serviceFile = new File(configBase, path);
      FileInputStream fisXML = new FileInputStream(serviceFile);
      Source xmlSource = new StreamSource(fisXML);
      InputStream fisXSL = null;
      if( transformBase != null )
      {
         File hostXSL = new File(transformBase, "default.xsl");
         fisXSL = new FileInputStream(hostXSL);
      }
      else
      {
         ClassLoader loader = Thread.currentThread().getContextClassLoader();
         URL hostXSL = loader.getResource("default.xsl");
         if( hostXSL == null )
            throw new FileNotFoundException("Failed to find resource: default.xsl");
         fisXSL = hostXSL.openStream();
      }
      Source xslSource = new StreamSource(fisXSL);
      // Generate the transformer.
      Transformer transformer = xsltFactory.newTransformer(xslSource);
      // Perform the transformation, sending the output to the response.
      transformer.transform(xmlSource, new StreamResult(out));
      out.close();
   }
   static void transformJarFile(ServletOutputStream out, String path)
      throws IOException, TransformerException
   {
      TransformerFactory xsltFactory = TransformerFactory.newInstance();
      // Get the XML input document and the stylesheet.
      File serviceFile = new File(configBase, path);
      JarFile sarFile = new JarFile(serviceFile);
      Manifest mf = sarFile.getManifest();
      Enumeration entries = sarFile.entries();
      JarOutputStream jos = new JarOutputStream(out, mf);
      File hostXSL = new File(transformBase, "default.xsl");
      byte[] buffer = new byte[2048];

      while( entries.hasMoreElements() )
      {
         JarEntry entry = (JarEntry) entries.nextElement();
         InputStream is = sarFile.getInputStream(entry);
         String name = entry.getName();
         log.trace("Begin entry: "+name+", size="+entry.getSize());
         if( name.equals("META-INF/MANIFEST.MF") )
            continue;
         if( name.endsWith(".xml") == false )
         {
            jos.putNextEntry(entry);
            // Copy the entry
            int totalBytes = 0;
            int bytes = is.read(buffer);
            while( bytes > 0 )
            {
               jos.write(buffer, 0, bytes);
               totalBytes += bytes;
               bytes = is.read(buffer);
            }
            is.close();
            jos.flush();
         }
         else
         {
            JarEntry newEntry = new JarEntry(name);
            newEntry.setTime(entry.getTime());
            jos.putNextEntry(newEntry);
            Source xmlSource = new StreamSource(is);
            FileInputStream fisXSL = new FileInputStream(hostXSL);
            Source xslSource = new StreamSource(fisXSL);
            // Generate the transformer.
            Transformer transformer = xsltFactory.newTransformer(xslSource);
            // Perform the transformation, sending the output to the response.
            transformer.transform(xmlSource, new StreamResult(jos));
         }
         jos.closeEntry();
         log.trace("End entry: "+name);
      }

      jos.close();
   }

}
