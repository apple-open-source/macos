/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.metadata;

import java.util.ArrayList;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.HashMap;

import java.lang.reflect.Field;

import javax.sql.DataSource;

import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.EntityMetaData;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.XmlLoadable;


/**
 * <description>
 *
 * @see <related>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:dirk@jboss.de">Dirk Zimmermann</a>
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @author <a href="mailto:menonv@cpw.co.uk">Vinay Menon</a>
 * @version $Revision: 1.17 $
 *
 *      Revisions:
 *      20010621 Bill Burke: made read-ahead defaultable in standardjboss.xml and jaws.xml
 *
 *
 */
public class JawsEntityMetaData
   extends MetaData
   implements XmlLoadable
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   /** Parent metadata structure. */
   private JawsApplicationMetaData jawsApplication;

   /** Parent metadata structure. */
   private EntityMetaData entity;

   /** The name of the bean (same as entity.getEjbName()). */
   private String ejbName = null;

   /** The name of the table to use for this bean. */
   private String tableName = null;

   /** Do we have to try and create the table on deployment? */
   private boolean createTable;

   /** Do we have to drop the table on undeployment? */
   private boolean removeTable;

   /** Do we use tuned updates? */
   private boolean tunedUpdates;

   /** Do we use do row locking on ejb loads? */
   private boolean rowLocking;

   /** Is the bean read-only? */
   private boolean readOnly;

   /** Make finders by default read-ahead? */
   private boolean readAhead;

   private int timeOut;

   /** Should the table have a primary key constraint? */
   private boolean pkConstraint;

   /** Is the bean's primary key a composite object? */
   private boolean compositeKey;

   /** The class of the primary key. */
   private Class primaryKeyClass;

   /** The fields we must persist for this bean. */
   private Hashtable cmpFields = new Hashtable();

   /** The fields that belong to the primary key (if composite). */
   private ArrayList pkFields = new ArrayList();

   /** Finders for this bean. */
   private ArrayList finders = new ArrayList();

   // the bean level datasource
   /**
    * This will now support datasources at the bean level. If no datasource
    * has been specified at the bean level then the global datasource is used
    *
    * This provides flexiblity for having single deployment units connecting to
    * different datasources for different beans.
    *
    */
   private DataSource dataSource=null;

   /**
    * Here we store the basename of all detailed fields in jaws.xml
    * (e.g., "data" for "data.categoryPK").
    */
   private HashMap detailedFieldDescriptions = new HashMap();

   /**
    * This is the Boolean we store as value in detailedFieldDescriptions.
    */
   private static final Boolean detailedBoolean = new Boolean(true);


   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   public JawsEntityMetaData(JawsApplicationMetaData app, EntityMetaData ent)
      throws DeploymentException
   {
      // initialisation of this object goes as follows:
      //  - constructor
      //  - importXml() for standardjaws.xml and jaws.xml

      jawsApplication = app;
      entity = ent;
      ejbName = entity.getEjbName();
      compositeKey = entity.getPrimKeyField() == null;

      try
      {
         primaryKeyClass =
            jawsApplication.getClassLoader().loadClass(entity.getPrimaryKeyClass());
      }
      catch (ClassNotFoundException e)
      {
         throw new DeploymentException(
            "could not load primary key class: " + entity.getPrimaryKeyClass());
      }

      // we replace the . by _ because some dbs die on it...
      // the table name may be overridden in importXml(jaws.xml)
      tableName = ejbName.replace('.', '_');

      // build the metadata for the cmp fields now in case there is no jaws.xml
      Iterator cmpFieldNames = entity.getCMPFields();

      while (cmpFieldNames.hasNext())
      {
         String cmpFieldName = (String)cmpFieldNames.next();
         CMPFieldMetaData cmpField = new CMPFieldMetaData(cmpFieldName, this);

         cmpFields.put(cmpFieldName, cmpField);
      }

      // build the pkfields metadatas
      if (compositeKey)
      {
         Field[] pkClassFields = primaryKeyClass.getFields();

         for (int i = 0; i < pkClassFields.length; i++) {
            Field pkField = pkClassFields[i];
            CMPFieldMetaData cmpField = (CMPFieldMetaData)cmpFields.get(pkField.getName());

            if (cmpField == null)
            {
               throw new DeploymentException(
                  "Bean " + ejbName + " has PK of type " + primaryKeyClass.getName() +
                  ", so it should have a cmp-field named " + pkField.getName());
            }

            pkFields.add(new PkFieldMetaData(pkField, cmpField, this));
         }
      }
      else
      {
         String pkFieldName = entity.getPrimKeyField();
         CMPFieldMetaData cmpField = (CMPFieldMetaData)cmpFields.get(pkFieldName);

         pkFields.add(new PkFieldMetaData(cmpField, this));
      }
   }

   // Public --------------------------------------------------------

   public JawsApplicationMetaData getJawsApplication() { return jawsApplication; }

   public EntityMetaData getEntity() { return entity; }

   public Iterator getCMPFields() { return cmpFields.values().iterator(); }

   public CMPFieldMetaData getCMPFieldByName(String name) {
      return (CMPFieldMetaData)cmpFields.get(name);
   }

   public Iterator getPkFields() { return pkFields.iterator(); }

   public int getNumberOfPkFields() { return pkFields.size(); }

   public String getTableName() { return tableName; }

   public boolean getCreateTable() { return createTable; }

   public boolean getRemoveTable() { return removeTable; }

   public boolean hasTunedUpdates() { return tunedUpdates; }

   public boolean hasPkConstraint() { return pkConstraint; }

   public int getReadOnlyTimeOut() { return timeOut; }

   public boolean hasCompositeKey() { return compositeKey; }

   // Return appropriate datasource
   public DataSource getDataSource()
   {
      // If a local datasource has been specified use it
      if(this.dataSource != null)
      {
         return dataSource;
      }
      // Use the gloabal datasource
      else
      {
         return jawsApplication.getDataSource();
      }
   }

   public String getDbURL() { return jawsApplication.getDbURL(); }

   public Iterator getFinders() { return finders.iterator(); }

   public String getName() { return ejbName; }

   public int getNumberOfCMPFields() { return cmpFields.size(); }

   public Class getPrimaryKeyClass() { return primaryKeyClass; }

   public boolean isReadOnly() { return readOnly; }

   public Iterator getEjbReferences() { return entity.getEjbReferences(); }

   public String getPrimKeyField() { return entity.getPrimKeyField(); }

   public boolean hasRowLocking() { return rowLocking; }

   public boolean hasReadAhead() { return readAhead; }

   // XmlLoadable implementation ------------------------------------

   public void importXml(Element element)
      throws DeploymentException
   {
      // This method will be called:
      //  - with element = <default-entity> from standardjaws.xml (always)
      //  - with element = <default-entity> from jaws.xml (if provided)
      //  - with element = <entity> from jaws.xml (if provided)

      // All defaults are set during the first call. The following calls override them.

      //get the bean level datasouce name
      String dataSourceName = getElementContent(getOptionalChild(element, "datasource"));

      //if a local datasource name is found bind it and set the local datasource
      if (dataSourceName!=null)
      {
         // Make sure it is prefixed with java:
         if (!dataSourceName.startsWith("java:/"))
         {
            dataSourceName = "java:/"+dataSourceName;

            // TODO: Authors, PLEASE have a look here, original indentation
            //       leads to the assumption, that the following IF only is
            //       evaluated, when the previous IF is true -- but the
            //       language doesn't look for indentation! Cleared that up
            //       by inserting a block. PLEASE VERYFY THAT I'M RIGHT!
         }
         // find the datasource
         if (! dataSourceName.startsWith("jdbc:"))
         {
            try
            {
               this.dataSource = (DataSource)new InitialContext().lookup(dataSourceName);
            }
            catch (NamingException e)
            {
               throw new DeploymentException(e.getMessage());
            }
         }
      }

      // get table name
      String tableStr = getElementContent(getOptionalChild(element, "table-name"));
      if (tableStr != null) tableName = tableStr;

      // create table?  If not provided, keep default.
      String createStr = getElementContent(getOptionalChild(element, "create-table"));
      if (createStr != null) createTable = Boolean.valueOf(createStr).booleanValue();

      // remove table?  If not provided, keep default.
      String removeStr = getElementContent(getOptionalChild(element, "remove-table"));
      if (removeStr != null) removeTable = Boolean.valueOf(removeStr).booleanValue();

      // tuned updates?  If not provided, keep default.
      String tunedStr = getElementContent(getOptionalChild(element, "tuned-updates"));
      if (tunedStr != null) tunedUpdates = Boolean.valueOf(tunedStr).booleanValue();

      // read only?  If not provided, keep default.
      String roStr = getElementContent(getOptionalChild(element, "read-only"));
      if (roStr != null) readOnly = Boolean.valueOf(roStr).booleanValue();

      // read ahead?  If not provided, keep default.
      String raheadStr = getElementContent(getOptionalChild(element, "read-ahead"));
      if (raheadStr != null) readAhead = Boolean.valueOf(raheadStr).booleanValue();

      String sForUpStr = getElementContent(getOptionalChild(element, "row-locking"));
      if (sForUpStr != null) rowLocking = (Boolean.valueOf(sForUpStr).booleanValue());
      rowLocking = rowLocking && !readOnly;

      // read only timeout?
      String toStr = getElementContent(getOptionalChild(element, "time-out"));
      if (toStr != null) timeOut = Integer.valueOf(toStr).intValue();

      // primary key constraint?  If not provided, keep default.
      String pkStr = getElementContent(getOptionalChild(element, "pk-constraint"));
      if (pkStr != null) pkConstraint = Boolean.valueOf(pkStr).booleanValue();

      // cmp fields
      Iterator iterator = getChildrenByTagName(element, "cmp-field");

      while (iterator.hasNext())
      {
         Element cmpField = (Element)iterator.next();
         String fieldName = getElementContent(getUniqueChild(cmpField, "field-name"));

         CMPFieldMetaData cmpFieldMetaData = getCMPFieldByName(fieldName);
         if (cmpFieldMetaData == null)
         {
            // Before we throw an exception, we have to check for nested cmp-fields.
            // We just add a new CMPFieldMetaData.
            if (isDetailedFieldDescription(fieldName))
            {
               // We obviously have a cmp-field like "data.categoryPK" in jaws.xml
               // and a cmp-field "data" in ejb-jar.xml.
               // In this case, we assume the "data.categoryPK" as a detailed description for "data".
               cmpFieldMetaData = new CMPFieldMetaData(fieldName, this);
               cmpFields.put(fieldName, cmpFieldMetaData);
            }
            else
            {
               throw new DeploymentException("cmp-field '"+fieldName+"' found in jaws.xml but not in ejb-jar.xml");
            }
         }
         cmpFieldMetaData.importXml(cmpField);
      }

      // finders
      iterator = getChildrenByTagName(element, "finder");

      while (iterator.hasNext())
      {
         Element finder = (Element)iterator.next();
         FinderMetaData finderMetaData = new FinderMetaData();
         finderMetaData.setReadAhead(readAhead);
         finderMetaData.importXml(finder);

         finders.add(finderMetaData);
      }

   }

   /**
    * @return true For a fieldname declared in jaws.xml like "data.categoryPK" if
    * there was a fieldname declared in ejb-jar.xml like "data".
    */
   private boolean isDetailedFieldDescription(String fieldName)
   {
      String fieldBaseName = CMPFieldMetaData.getFirstComponent(fieldName);

      if (detailedFieldDescriptions.containsKey(fieldBaseName))
      {
         return true;
      }

      CMPFieldMetaData cmpFieldMetaData = getCMPFieldByName(fieldBaseName);
      if (cmpFieldMetaData == null)
      {
         return false;
      }
      else
      {
         detailedFieldDescriptions.put(fieldBaseName, detailedBoolean);
         cmpFields.remove(fieldBaseName);
         return true;
      }
   }


   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
