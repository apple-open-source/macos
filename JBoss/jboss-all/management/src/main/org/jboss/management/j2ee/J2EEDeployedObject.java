/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.j2ee;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.StringWriter;
import java.net.URL;
import java.net.URLClassLoader;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.J2EEDeployedObject J2EEDeployedObject}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.8.2.1 $
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 */
public abstract class J2EEDeployedObject
      extends J2EEManagedObject
      implements J2EEDeployedObjectMBean
{
   // Constants -----------------------------------------------------

   public static final int APPLICATION = 0;
   public static final int WEB = 1;
   public static final int EJB = 2;
   public static final int RAR = 3;
   public static final int SAR = 4;
   public static final int JBOSS = 5;
   public static final int JAWS = 6;
   public static final int CMP = 7;
   public static final int JBOSS_WEB = 8;

   private static final String[] sDescriptors = new String[]{
      "META-INF/application.xml",
      "WEB-INF/web.xml",
      "META-INF/ejb-jar.xml",
      "META-INF/ra.xml",
      "META-INF/jboss-service.xml",
      "META-INF/jboss.xml",
      "META-INF/jaws.xml",
      "META-INF/jbosscmp-jdbc.xml",
      "WEB-INF/jboss-web.xml",
   };

   // Attributes ----------------------------------------------------

   private String mDeploymentDescriptor;

   // Static --------------------------------------------------------

   public static String getDeploymentDescriptor(URL pJarUrl, int pType)
   {
      Logger log = Logger.getLogger(J2EEDeployedObject.class);

      if (pJarUrl == null)
      {
         // Return if the given URL is null
         return "";
      }
      String lDD = null;
      Reader lInput = null;
      StringWriter lOutput = null;
      try
      {
         if (pJarUrl.toString().endsWith("service.xml"))
         {
            lInput = new InputStreamReader(pJarUrl.openStream());
         }
         else
         {
            // First get the deployement descriptor
            log.debug("File: " + pJarUrl + ", descriptor: " + sDescriptors[pType]);
            ClassLoader localCl = new URLClassLoader(new URL[]{pJarUrl});
            InputStream lStream = localCl.getResourceAsStream(sDescriptors[pType]);
            if (lStream == null)
            {
               // If DD not found then return a null indicating the file is not available
               return null;
            }
            lInput = new InputStreamReader(lStream);
         }
         lOutput = new StringWriter();
         char[] lBuffer = new char[1024];
         int lLength = 0;
         while ((lLength = lInput.read(lBuffer)) > 0)
         {
            lOutput.write(lBuffer, 0, lLength);
         }
         lDD = lOutput.toString();
      }
      catch (Exception e)
      {
         log.error("failed to get deployment descriptor", e);
      }
      finally
      {
         if (lInput != null)
         {
            try
            {
               lInput.close();
            }
            catch (Exception e)
            {
            }
         }
         if (lOutput != null)
         {
            try
            {
               lOutput.close();
            }
            catch (Exception e)
            {
            }
         }
      }
      return lDD;
   }

   // Constructors --------------------------------------------------

   /**
    * Constructor taking the Name of this Object
    *
    * @param pName Name to be set which must not be null
    * @param pDeploymentDescriptor
    *
    * @throws InvalidParameterException If the given Name is null
    */
   public J2EEDeployedObject(
         String pType,
         String pName,
         ObjectName pParent,
         String pDeploymentDescriptor
         )
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(pType, pName, pParent);
      mDeploymentDescriptor = pDeploymentDescriptor;
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.J2EEDeployedObject implementation -------

   /**
    * @jmx:managed-attribute
    */
   public String getDeploymentDescriptor()
   {
      return mDeploymentDescriptor;
   }

   /**
    * @jmx:managed-attribute
    */
   public ObjectName getServer1()
   {
      //AS ToDo: Need to be implemented
      return null;
   }

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "J2EEDeployedObject { " + super.toString() + " } [ " +
            "deployment descriptor: " + mDeploymentDescriptor +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
