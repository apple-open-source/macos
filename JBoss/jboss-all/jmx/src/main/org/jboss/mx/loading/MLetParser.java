/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.loading;

import java.util.Set;
import java.util.HashSet;
import java.util.StringTokenizer;
import java.util.NoSuchElementException;

import java.net.URL;
import java.net.MalformedURLException;

import java.text.ParseException;

import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;

import org.jboss.logging.Logger;

/**
 * Parses an MLet text file confirming to spec format.
 *
 * @see javax.management.MLet
 * @see javax.management.MBeanFileParser
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.6.4.1 $  
 */
public class MLetParser
   implements MBeanFileParser
{
   private static final Logger log = Logger.getLogger(MLetParser.class);

   // MBeanFileParser implementation --------------------------------
   
   /**
    * Reads an MLet text file from a given URL.
    *
    * @param   url   URL to MLet text file
    * @return  set containing <tt>MBeanElement</tt> objects representing
    *          the parsed MBean entries
    * @throws  ParseException if there was error in reading the MLet text file 
    * @throws  MalformedURLException if the <tt>url</tt> argument cannot be used to 
    *          construct a valid URL.
    */
   public Set parseMBeanFile(String url) throws ParseException, MalformedURLException
   {
      return parseMBeanFile(new URL(url));
   }

   /**
    * Reads an MLet text file from a given URL.
    *
    * @param   url   URL to MLet text file
    * @return  set containing <tt>MBeanElement</tt> objects representing the parsed
    *          MBean entries
    * @throws  ParseException if there was an error in reading the MLet text file
    */
   public Set parseMBeanFile(URL url) throws ParseException
   {
      Set mlets            = new HashSet(); 
      MBeanElement element = null;

      try
      {         
         BufferedReader reader = new BufferedReader(new InputStreamReader(url.openStream()));
         int c                 = -1;
      
         // read the file
         while((c = reader.read()) != -1)
         {
            // read and parse one tag at a time
            if (c == '<')
            {            
               StringBuffer buf = new StringBuffer(1000);
               boolean readMore = true;
               
               // read the element contents
               while(readMore)
               {
                  c = reader.read();
                  
                  if (c == -1)
                     throw new ParseException("Unexpected end of file. Tag was not closed: " + buf.toString().replace('\t', ' ').replace('\n', ' ').replace('\r', ' ').trim(), 0);
                  
                  if (c == '>')
                  {
                     readMore = false;
                     break;
                  }
                  
                  buf.append((char)c);
               }

               // tokenize the element contents
               StringTokenizer tokenizer = new StringTokenizer(buf.toString(), "= \n\t\r");
               String tagName = null, attributeName = null, attributeValue = null;
               
               // first token is the tag name
               if (tokenizer.hasMoreTokens())
                  tagName = tokenizer.nextToken().trim();
               
               // parse MLET tag
               if (tagName.equals("MLET"))
               {
                  element = new MBeanElement();
                  
                  while(tokenizer.hasMoreTokens())
                  {
                     try
                     {
                        // following tokens are attribute name=value pairs
                        attributeName = tokenizer.nextToken("= \n\t\r").trim();
                        attributeValue = tokenizer.nextToken(" \n\t\r").trim();
                     
                        if (attributeValue.equals("="))
                           attributeValue = tokenizer.nextToken();
                        
                        // CODE attribute
                        if (attributeName.equals("CODE"))
                           element.setCode(attributeValue);
                        
                        // OBJET attribute
                        else if (attributeName.equals("OBJECT"))
                           element.setObject(attributeValue);
                        
                        // ARCHIVE attribute
                        else if (attributeName.equals("ARCHIVE"))
                           // FIXME: according to spec "archivelist" must be in quotes, we don't enforce that
                           element.setArchive(attributeValue);
                        
                        // CODEBASE attribute
                        else if (attributeName.equals("CODEBASE"))
                           element.setCodebase(attributeValue);
                        
                        // NAME attribute
                        else if (attributeName.equals("NAME"))
                           element.setName(attributeValue);
                        
                        // VERSION attribute
                        else if (attributeName.equals("VERSION"))
                           element.setVersion(attributeValue);
                     }
                     catch (NoSuchElementException e)
                     {
                        // couldn't find a valid attribute, value pair
                        // ignore and move to next one
                        
                        log.warn("No value found for attribute '" + attributeName);
                     }
                  }
                  
                  if (element.getCode() == null && element.getObject() == null)
                     throw new ParseException("<" + buf.toString().replace('\n', ' ').replace('\r', ' ').replace('\t', ' ').trim() + "> is missing mandatory CODE | OBJECT attribute", 0);
                  if (element.getArchives().size() == 0)
                     throw new ParseException("<" + buf.toString().replace('\n', ' ').replace('\r', ' ').replace('\t', ' ').trim() + "> is missing mandatory ARCHIVE attribute", 0);
               }
               
               // parse </MLET> tag
               else if (tagName.equals("/MLET"))
               {
                  mlets.add(element);
                  element = null;
               }
           
               // parse <ARG> tag
               else if (tagName.equals("ARG"))
               {
                  try
                  {
                     // if second token is not TYPE then skip the attribute
                     if (!tokenizer.nextToken().equals("TYPE"))
                        continue;
                        
                     String type = tokenizer.nextToken();
                     
                     // if fourth token is not VALUE then skip the attribute
                     if (!tokenizer.nextToken().equals("VALUE"))
                        continue;
                        
                     String value = tokenizer.nextToken(" \n\t\r");
                     
                     // element is non-null if we're within <MLET> </MLET> tags
                     if (element != null)
                        element.addArg(type, value);
                  }
                  catch (NoSuchElementException e)
                  {
                     // malformed ARG tag means the MBean can't be instantiated
                     element = null;
                     
                     log.warn("Malformed element: <" + buf.toString() + ">");
                  }
               }               
            }  // end of if (c == '<')
         }  // while((c = reader.read()) != -1) 
         
         // get rid of any null elements
         mlets.remove(null);
         return mlets;
      }
      catch (IOException e) 
      {
         throw new ParseException(e.toString(), 0);
      }
   }
   
}
      



