/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.metadata;

import java.util.Hashtable;
import java.util.Iterator;

import javax.sql.DataSource;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;

import org.jboss.logging.Logger;

import org.jboss.metadata.XmlLoadable;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.BeanMetaData;
import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.ApplicationMetaData;

/**
 * <description>
 *
 * @see <related>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @version $Revision: 1.14 $
 *
 *   <p><b>Revisions:</b>
 *
 *   <p><b>20010812 vincent.harcq@hubmethods.com:</b>
 *   <ul>
 *   <li> Get Rid of debug flag, use log4j instead
 *   </ul>
 *
 */
public class JawsApplicationMetaData extends MetaData implements XmlLoadable {
   // Constants -----------------------------------------------------

   public static final String JPM = "org.jboss.ejb.plugins.jaws.JAWSPersistenceManager";

   // Attributes ----------------------------------------------------

   /**
    * The classloader comes from the container. It is used to load
    * the classes of the beans and their primary keys.
    */
   private ClassLoader classLoader;

   /** The "parent" applicationmetadata. */
   private ApplicationMetaData applicationMetaData;

   /** This only contains the jaws-managed cmp entities. */
   private Hashtable entities = new Hashtable();

   /** The datasource to use for this application. */
   private String dbURL;
   private DataSource dataSource;

   /** All the available type mappings. */
   private Hashtable typeMappings = new Hashtable();

   /** The type mapping to use with the specified database. */
   private TypeMappingMetaData typeMapping;

   private Logger log = Logger.getLogger(JawsApplicationMetaData.class);

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public JawsApplicationMetaData(ApplicationMetaData amd, ClassLoader cl)
      throws DeploymentException
   {
      // initialisation of this object goes as follows:
      //  - constructor
      //  - importXml() for standardjaws.xml and jaws.xml
      //  - create()

      // the classloader is the same for all the beans in the application
      classLoader = cl;
      applicationMetaData = amd;

      // create metadata for all jaws-managed cmp entities
      // we do that here in case there is no jaws.xml
      Iterator beans = applicationMetaData.getEnterpriseBeans();
      while (beans.hasNext())
      {
         BeanMetaData bean = (BeanMetaData)beans.next();

         // only take entities
         if (bean.isEntity())
         {
            EntityMetaData entity = (EntityMetaData)bean;

            // only take jaws-managed CMP entities
            if (entity.isCMP() &&
                entity.getContainerConfiguration().getPersistenceManager().equals(JPM))
            {
               JawsEntityMetaData jawsEntity = new JawsEntityMetaData(this, entity);
               entities.put(entity.getEjbName(), jawsEntity);
            }
         }
      }
   }
   
   // Public --------------------------------------------------------

   public DataSource getDataSource() { return dataSource; }

   public String getDbURL() { return dbURL; }

   public TypeMappingMetaData getTypeMapping() { return typeMapping; }

   protected ClassLoader getClassLoader() { return classLoader; }

   public JawsEntityMetaData getBeanByEjbName(String name)
   {
      return (JawsEntityMetaData)entities.get(name);
   }

   public void create()
      throws DeploymentException
   {
      // find the datasource
      if (! dbURL.startsWith("jdbc:")) {
         try {
            dataSource = (DataSource)new InitialContext().lookup(dbURL);
         } catch (NamingException e) {
            throw new DeploymentException(e.getMessage());
         }
      }
   }

   // XmlLoadable implementation ------------------------------------

   public void importXml(Element element)
      throws DeploymentException
   {
      // importXml will be called at least once: with standardjaws.xml
      // it may be called a second time with user-provided jaws.xml
      // we must ensure to set all defaults values in the first call
      Iterator iterator;

      // first get the type mappings. (optional, but always set in standardjaws.xml)
      Element typeMaps = getOptionalChild(element, "type-mappings");

      if (typeMaps != null)
      {
         iterator = getChildrenByTagName(typeMaps, "type-mapping-definition");

         while (iterator.hasNext())
         {
            Element typeMappingElement = (Element)iterator.next();
            TypeMappingMetaData typeMapping = new TypeMappingMetaData();
            try
            {
               typeMapping.importXml(typeMappingElement);
            }
            catch (DeploymentException e)
            {
               throw new DeploymentException(
                  "Error in jaws.xml for type-mapping-definition " +
                  typeMapping.getName() + ": " + e.getMessage()
               );
            }
            typeMappings.put(typeMapping.getName(), typeMapping);
         }
      }

      // get the datasource (optional, but always set in standardjaws.xml)
      Element db = getOptionalChild(element, "datasource");
      if (db != null) dbURL = getElementContent(db);

      // Make sure it is prefixed with java:
      if (!dbURL.startsWith("java:/"))
         dbURL = "java:/"+dbURL;

      // get the type mapping for this datasource
      // (optional, but always set in standardjaws.xml)
      String typeMappingString =
         getElementContent(getOptionalChild(element, "type-mapping"));

      if (typeMappingString != null)
      {
         typeMapping = (TypeMappingMetaData)typeMappings.get(typeMappingString);

         if (typeMapping == null)
         {
            throw new DeploymentException(
               "Error in jaws.xml : type-mapping " + typeMappingString + " not found");
         }
      }

      // get default settings for the beans (optional, but always set in standardjaws.xml)
      Element defaultEntity = getOptionalChild(element, "default-entity");

      if (defaultEntity != null)
      {
         iterator = entities.values().iterator();

         while (iterator.hasNext())
         {
            ((JawsEntityMetaData)iterator.next()).importXml(defaultEntity);
         }
      }

      // get the beans data (only in jaws.xml)
      Element enterpriseBeans = getOptionalChild(element, "enterprise-beans");

      if (enterpriseBeans != null)
      {
         String ejbName = null;

         try
         {
            iterator = getChildrenByTagName(enterpriseBeans, "entity");

            while (iterator.hasNext())
            {
               Element bean = (Element) iterator.next();
               ejbName = getElementContent(getUniqueChild(bean, "ejb-name"));
               JawsEntityMetaData entity = (JawsEntityMetaData)entities.get(ejbName);

               if (entity != null)
               {
                  entity.importXml(bean);
               }
               else
               {
                  log.warn(
                     "Warning: data found in jaws.xml for entity " + ejbName +
                     " but bean is not a jaws-managed cmp entity in ejb-jar.xml"
                  );
               }
            }

         }
         catch (DeploymentException e)
         {
            throw new DeploymentException(
               "Error in jaws.xml for Entity " + ejbName + ": " + e.getMessage());
         }
      }
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
