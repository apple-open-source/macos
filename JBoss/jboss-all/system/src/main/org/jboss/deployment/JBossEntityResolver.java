package org.jboss.deployment;

import java.util.Hashtable;
import java.io.InputStream;
import org.xml.sax.EntityResolver;
import org.xml.sax.InputSource;

/** Local entity resolver to handle standard J2EE DTDs as well as JBoss
 * specific DTDs.
 *
 * Function boolean hadDTD() is here to avoid validation errors in
 * descriptors that do not have a DOCTYPE declaration.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.3.2.3 $
 */
public class JBossEntityResolver implements EntityResolver
{
   private static Hashtable dtds = new Hashtable();
   private boolean hasDTD = false;

   static
   {
      registerDTD("-//Sun Microsystems, Inc.//DTD Enterprise JavaBeans 1.1//EN", "ejb-jar.dtd");
      registerDTD("-//Sun Microsystems, Inc.//DTD Enterprise JavaBeans 2.0//EN", "ejb-jar_2_0.dtd");
      registerDTD("-//Sun Microsystems, Inc.//DTD J2EE Application 1.2//EN", "application_1_2.dtd");
      registerDTD("-//Sun Microsystems, Inc.//DTD J2EE Application 1.3//EN", "application_1_3.dtd");
      registerDTD("-//Sun Microsystems, Inc.//DTD J2EE Application Client 1.3//EN", "application-client_1_3.dtd");
      registerDTD("-//Sun Microsystems, Inc.//DTD Connector 1.0//EN", "connector_1_0.dtd");
      registerDTD("-//Sun Microsystems, Inc.//DTD Web Application 2.2//EN", "web-app_2_2.dtd");
      registerDTD("-//Sun Microsystems, Inc.//DTD Web Application 2.3//EN", "web-app_2_3.dtd");
      registerDTD("-//JBoss//DTD J2EE Application 1.3//EN", "jboss-app_3_0.dtd");
      registerDTD("-//JBoss//DTD J2EE Application 1.3V2//EN", "jboss-app_3_2.dtd");
      registerDTD("-//JBoss//DTD JAWS//EN", "jaws.dtd");
      registerDTD("-//JBoss//DTD JAWS 2.4//EN", "jaws_2_4.dtd");
      registerDTD("-//JBoss//DTD JAWS 3.0//EN", "jaws_3_0.dtd");
      registerDTD("-//JBoss//DTD JBOSS//EN", "jboss.dtd");
      registerDTD("-//JBoss//DTD JBOSS 2.4//EN", "jboss_2_4.dtd");
      registerDTD("-//JBoss//DTD JBOSS 3.0//EN", "jboss_3_0.dtd");
      registerDTD("-//JBoss//DTD JBOSS 3.2//EN", "jboss_3_2.dtd");
      registerDTD("-//JBoss//DTD JBOSSCMP-JDBC 3.0//EN", "jbosscmp-jdbc_3_0.dtd");
      registerDTD("-//JBoss//DTD JBOSSCMP-JDBC 3.2//EN", "jbosscmp-jdbc_3_2.dtd");
      registerDTD("-//JBoss//DTD Web Application 2.2//EN", "jboss-web.dtd");
      registerDTD("-//JBoss//DTD Web Application 2.3//EN", "jboss-web_3_0.dtd");
      registerDTD("-//JBoss//DTD Web Application 2.3V2//EN", "jboss-web_3_2.dtd");
      registerDTD("-//JBoss//DTD MBean Service 3.2//EN", "jboss-service_3_2.dtd");
   }

   /** Register the mapping from the public id to the dtd file name.
    *
    * @param publicId the DOCTYPE public id, "-//Sun Microsystems, Inc.//DTD Enterprise JavaBeans 1.1//EN"
    * @param dtdFileName the simple dtd file name, "ejb-jar.dtd"
    */
   public static void registerDTD(String publicId, String dtdFileName)
   {
      dtds.put(publicId, dtdFileName);
   }

   /** Register the mapping of the DOCTYPE public ID names to the DTD file
    */
   public JBossEntityResolver()
   {
   }

   /**
    * Returns DTD inputSource. If DTD was found in the hashtable and inputSource
    * was created flag hasDTD is set to true.
    *
    * @param String publicId - Public ID of DTD
    * @param String systemId - the system ID of DTD
    * @return InputSource of DTD
    */
   public InputSource resolveEntity(String publicId, String systemId)
   {
      hasDTD = false;
      String dtd = null;
      if( publicId != null )
         dtd = (String) dtds.get(publicId);

      if (dtd != null)
      {
         hasDTD = true;
         try
         {
            ClassLoader loader = Thread.currentThread().getContextClassLoader();
            // The DTDs are expected to be in the org/jboss/metadata package
            String dtdResource = "org/jboss/metadata/" + dtd;
            InputStream dtdStream = loader.getResourceAsStream(dtdResource);
            InputSource aInputSource = new InputSource(dtdStream);
            return aInputSource;
         }
         catch (Exception ignore)
         {
         }
      }
      return null;
   }

   /**
    * Returns the boolean value to inform id DTD was found in the XML file or not
    *
    * @return boolean - true if DTD was found in XML
    */
   public boolean hasDTD()
   {
      return hasDTD;
   }
}
