/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import org.w3c.dom.Element;
import org.jboss.deployment.DeploymentException;
import org.jboss.logging.Logger;
import org.jboss.util.Strings;

/**
 * The meta data information specific to entity beans.
 *
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
 * @author <a href="mailto:criege@riege.com">Christian Riege</a>
 *
 * @version $Revision: 1.19.2.3 $
 *
 * <p><b>Revisions:</b><br>
 * <p><b>2001/10/16: billb</b>
 * <ol>
 *  <li>Added clustering tags
 * </ol>
 */
public class EntityMetaData
   extends BeanMetaData
{
   // Constants -----------------------------------------------------
   public final static int CMP_VERSION_1 = 1;
   public final static int CMP_VERSION_2 = 2;
   public static final String DEFAULT_ENTITY_INVOKER_PROXY_BINDING =
      "entity-rmi-invoker";
   public static final String DEFAULT_CLUSTERED_ENTITY_INVOKER_PROXY_BINDING =
      "clustered-entity-rmi-invoker";

   // Attributes ----------------------------------------------------
   private boolean cmp;
   private String primaryKeyClass;
   private boolean reentrant;
   private int cmpVersion;
   private String abstractSchemaName;
   private ArrayList cmpFields = new ArrayList();
   private String primKeyField;
   private ArrayList queries = new ArrayList();
   private boolean readOnly = false;
   private boolean doDistCachInvalidations = false;
   private CacheInvalidationConfigMetaData cacheInvalidConfig = null; 

   // Static --------------------------------------------------------
   private static Logger log = Logger.getLogger( EntityMetaData.class );

   // Constructors --------------------------------------------------
   public EntityMetaData( ApplicationMetaData app )
   {
      super(app, BeanMetaData.ENTITY_TYPE);
   }

   // Public --------------------------------------------------------
   public boolean isCMP()
   {
      return cmp;
   }

   public boolean isCMP1x()
   {
      return cmp && (cmpVersion==1);
   }

   public boolean isCMP2x()
   {
      return cmp && (cmpVersion==2);
   }

   public boolean isBMP()
   {
      return !cmp;
   }

   public String getPrimaryKeyClass()
   {
      return primaryKeyClass;
   }

   public boolean isReentrant()
   {
      return reentrant;
   }

   public String getAbstractSchemaName()
   {
      return abstractSchemaName;
   }

   public boolean isReadOnly()
   {
      return readOnly;
   }

   /**
    * Gets the container managed fields.
    * @returns iterator over Strings containing names of the fields
    */
   public Iterator getCMPFields()
   {
      return cmpFields.iterator();
   }

   public String getPrimKeyField()
   {
      return primKeyField;
   }

   public Iterator getQueries()
   {
      return queries.iterator();
   }

   public String getDefaultConfigurationName()
   {
      if (isCMP())
      {
         if(getApplicationMetaData().isEJB2x())
         {
            if (isClustered())
            {
               return ConfigurationMetaData.CLUSTERED_CMP_2x_13;
            }
            else
            {
               return ConfigurationMetaData.CMP_2x_13;
            }
         }
         else
         {
            if (isClustered())
            {
               return ConfigurationMetaData.CLUSTERED_CMP_1x_13;
            }
            else
            {
               return ConfigurationMetaData.CMP_1x_13;
            }
         }
      }
      else
      {
         if (isClustered())
         {
            return ConfigurationMetaData.CLUSTERED_BMP_13;
         }
         else
         {
            return ConfigurationMetaData.BMP_13;
         }
      }
   }
   
   public boolean doDistributedCacheInvalidations ()
   {
      return this.doDistCachInvalidations ;
   }
   
   public CacheInvalidationConfigMetaData getDistributedCacheInvalidationConfig ()
   {
      return this.cacheInvalidConfig ;
   }

   public void importEjbJarXml( Element element )
      throws DeploymentException
   {
      super.importEjbJarXml(element);

      // set persistence type
      String persistenceType = getElementContent(getUniqueChild(element,
         "persistence-type"));
      if( persistenceType.equals("Bean") )
      {
         cmp = false;
      }
      else if( persistenceType.equals("Container") )
      {
         cmp = true;
      }
      else
      {
         throw new DeploymentException( getEjbName() +  ": " +
            "persistence-type must be 'Bean' or 'Container'!" );
      }

      // set primary key class
      primaryKeyClass = getElementContent(getUniqueChild(element,
         "prim-key-class"));

      // set reentrant
      reentrant = Boolean.valueOf(getElementContent(getUniqueChild(element,
         "reentrant"))).booleanValue();

      if( isCMP() )
      {
         // cmp-version
         if( getApplicationMetaData().isEJB2x() )
         {
            String cmpVersionString = getElementContent(
               getOptionalChild(element, "cmp-version"));

            if( cmpVersionString == null )
            {
               // default for ejb 2.0 apps is cmp 2.x
               cmpVersion = CMP_VERSION_2;
            }
            else
            {
               if( "1.x".equals(cmpVersionString) )
               {
                  cmpVersion = 1;
               }
               else if( "2.x".equals(cmpVersionString) )
               {
                  cmpVersion = 2;
               }
               else
               {
                  throw new DeploymentException( getEjbName() + ": " +
                     "cmp-version must be '1.x' or '2.x', if specified" );
               }
            }
         }
         else
         {
            // default for 1.0 DTDs is version 2
            cmpVersion = CMP_VERSION_1;
         }

         // abstract-schema-name
         abstractSchemaName = getOptionalChildContent(element,
            "abstract-schema-name");

         if( cmpVersion == 2 )
         {
            // Enforce several restrictions on abstract-schema-name and
            // ejb-name Elements, see bug #613360

            String ejbName = getEjbName();

            // ejb-name tests
            if( !Strings.isValidJavaIdentifier(ejbName) )
            {
               throw new DeploymentException( "The ejb-name for a CMP" +
                  "2.x Entity must be a valid Java Identifier" );
            }

            if( Strings.isEjbQlIdentifier(ejbName) )
            {
               log.warn( ejbName + ": The ejb-name for a CMP 2.x Entity " +
                  "should not be a reserved EJB-QL keyword" );
            }

            // Test various things for abstract-schema-name
            if( abstractSchemaName == null )
            {
               throw new DeploymentException( "The abstract-schema-name " +
                  "must be specified for CMP 2.x Beans" );
            }

            if( !Strings.isValidJavaIdentifier(abstractSchemaName) )
            {
               throw new DeploymentException( "The abstract-schema-name " +
                  "must be a valid Java Identifier '" + abstractSchemaName +
                  "'");
            }

            if( Strings.isEjbQlIdentifier(abstractSchemaName) )
            {
               log.warn( ejbName + ": The abstract-schema-name should " +
                  "not be a reserved EJB-QL Identifier '" +
                  abstractSchemaName + "'" );
            }
         }

         // cmp-fields
         Iterator iterator = getChildrenByTagName( element, "cmp-field" );
         while( iterator.hasNext() )
         {
            Element field = (Element)iterator.next();
            cmpFields.add(getElementContent(getUniqueChild(field,
               "field-name")));
         }

         // set the primary key field
         primKeyField = getElementContent(getOptionalChild(element,
            "primkey-field"));
         if( primKeyField != null && !cmpFields.contains(primKeyField) )
         {
            // FIXME: include ejb-name
            throw new DeploymentException( "primkey-field " + primKeyField +
               " is not a cmp-field");
         }

         // queries
         iterator = getChildrenByTagName(element, "query");
         while( iterator.hasNext() )
         {
            Element queryElement = (Element) iterator.next();

            QueryMetaData queryMetaData = new QueryMetaData();
            queryMetaData.importEjbJarXml(queryElement);

            queries.add(queryMetaData);
         }
      }
   }

   protected void defaultInvokerBindings()
   {
      this.invokerBindings = new HashMap();
      if( isClustered() )
      {
         this.invokerBindings.put(
            DEFAULT_CLUSTERED_ENTITY_INVOKER_PROXY_BINDING, getJndiName());
      }
      else
      {
         this.invokerBindings.put(
            DEFAULT_ENTITY_INVOKER_PROXY_BINDING, getJndiName());
      }
   }

   public void importJbossXml( Element element )
      throws DeploymentException
   {
      super.importJbossXml(element);
      // set readonly
      String readOnlyString = getElementContent(getOptionalChild(
         element, "read-only"));
      if( readOnlyString != null )
      {
         readOnly = Boolean.valueOf(readOnlyString).booleanValue();
      }
      // Manage distributed cache-invalidation settings
      //
      String distCacheInvalidations = getElementContent(getOptionalChild( element,
         "cache-invalidation"), (this.doDistCachInvalidations ? "True" : "False") );
      this.doDistCachInvalidations = distCacheInvalidations.equalsIgnoreCase ("True");

      Element cacheInvalidConfigElement = getOptionalChild(element,
         "cache-invalidation-config");

      this.cacheInvalidConfig = new CacheInvalidationConfigMetaData();
      this.cacheInvalidConfig.init(this);
      if (cacheInvalidConfigElement != null)
      {
         this.cacheInvalidConfig.importJbossXml(cacheInvalidConfigElement);
      }

      
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
/*
vim:ts=3:sw=3:et
*/
