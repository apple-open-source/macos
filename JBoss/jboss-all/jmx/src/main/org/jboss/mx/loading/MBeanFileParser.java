/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.loading;

import java.util.Set;
import java.net.URL;
import java.net.MalformedURLException;
import java.text.ParseException;

/**
 * Interface that abstracts the access to different MBean loader parsers
 * (MLet file parsers, XML based parsers, etc.).
 *
 * @see javax.management.loading.MLet
 * @see org.jboss.mx.loading.MLetParser
 * @see org.jboss.mx.loading.MBeanLoader
 * @see org.jboss.mx.loading.XMLMBeanParser
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public interface MBeanFileParser
{

   /**
    * Parses a file that describes the configuration of MBeans to load and
    * instantiate in the MBean server (for example, MLet text file).
    *
    * @see     org.jboss.mx.loading.MBeanElement
    *
    * @param   url   URL of the file
    * @return  a set of <tt>MBeanElement</tt> objects that contain the required
    *          information to load and register the MBean
    * @throws  ParseException if there was an error parsing the file
    */
   Set parseMBeanFile(URL url) throws ParseException;
   
   /**
    * Parses a file that describes the configuration of MBean to load and
    * instantiate in the MBean server (for example, MLet text file).
    *
    * @see     org.jboss.mx.loading.MBeanElement
    *
    * @param   url   URL of the file
    * @return  a set of <tt>MBeanElement</tt> objects that contain the required
    *          information to load and register the MBean
    * @throws  ParseException if there was an error parsing the file
    * @throws  MalformedURLException if the URL string was not valid
    */
   Set parseMBeanFile(String url) throws ParseException, MalformedURLException;
   
}
      



