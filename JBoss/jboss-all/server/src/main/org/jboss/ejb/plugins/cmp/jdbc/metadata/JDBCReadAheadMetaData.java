/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.util.Arrays;
import java.util.List;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;

import org.w3c.dom.Element;

/**
 * Imutable class which holds all the information about read-ahead settings.
 * It loads its data from standardjbosscmp-jdbc.xml and jbosscmp-jdbc.xml
 *
 * @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 * @version $Revision: 1.6.4.2 $
 */
public final class JDBCReadAheadMetaData {

   public static final JDBCReadAheadMetaData DEFAULT = new JDBCReadAheadMetaData();

   /*
    * Constants for read ahead strategy
    */
   /**
    * Don't read ahead.
    */
   private static final byte NONE = 0;

   /**
    * Read ahead when some entity is being loaded (lazily, good for 
    * all queries).
    */
   private static final byte ON_LOAD = 1;

   /**
    * Read ahead during "find" (not lazily, the best for queries with 
    * small result set).
    */
   private static final byte ON_FIND = 2;


   private static final List STRATEGIES = 
         Arrays.asList(new String[] {"none", "on-load", "on-find"});

   /**
    * The strategy of reading ahead, one of 
    * {@link #NONE}, {@link #ON_LOAD}, {@link #ON_FIND}.
    */
   private final byte strategy;

   /**
    * The page size of the read ahead buffer
    */
   private final int pageSize;

   /**
    * The name of the load group to eager load.
    */
   private final String eagerLoadGroup;

   /**
    * add this to a deeper left joined query
    */
   private boolean deepReadAhead = false;

   /**
    * Constructs default read ahead meta data: no read ahead.
    */
   private JDBCReadAheadMetaData() {
      strategy = ON_LOAD;
      pageSize = 255;
      eagerLoadGroup = "*";
   }

   /**
    * Constructs read ahead meta data with specified strategy, pageSize and 
    * eagerLoadGroup.
    */
   public JDBCReadAheadMetaData(
         String strategy, 
         int pageSize, 
         String eagerLoadGroup) {
      
      this.strategy = (byte) STRATEGIES.indexOf(strategy);
      if(this.strategy < 0) {
         throw new IllegalArgumentException("Unknown read ahead strategy '" + 
               strategy + "'.");
      }
      this.pageSize = pageSize;
      this.eagerLoadGroup = eagerLoadGroup;
   }

   /**
    * Constructs read ahead meta data with the data contained in the read-ahead
    * xml element from a jbosscmp-jdbc xml file. Optional values of the xml 
    * element that are not present are instead loaded from the defalutValues
    * parameter.
    *
    * @param element the xml Element which contains the read-ahead metadata
    * @throws DeploymentException if the xml element is invalid
    */
   public JDBCReadAheadMetaData(
         Element element,
         JDBCReadAheadMetaData defaultValue) throws DeploymentException {

      // Strategy
      String strategyStr = MetaData.getUniqueChildContent(element, "strategy");
      strategy = (byte) STRATEGIES.indexOf(strategyStr);
      if(strategy < 0) {
         throw new DeploymentException("Unknown read ahead strategy '" + 
               strategyStr + "'.");
      }
      if (MetaData.getOptionalChild(element, "deep-read-ahead") != null)
      {
         deepReadAhead = true;
      }

      // page-size
      String pageSizeStr = 
            MetaData.getOptionalChildContent(element, "page-size");
      if(pageSizeStr != null) {
         try {
            pageSize = Integer.parseInt(pageSizeStr);
         } catch (NumberFormatException ex) {
            throw new DeploymentException("Invalid number format in read-" +
                  "ahead page-size '" + pageSizeStr + "': " + ex);
         }
         if(pageSize < 0) {
            throw new DeploymentException("Negative value for read ahead " +
                  "page-size '" + pageSizeStr + "'.");
         }
      } else {
         pageSize = defaultValue.getPageSize();
      }

      // eager-load-group
      Element eagerLoadGroupElement = 
            MetaData.getOptionalChild(element, "eager-load-group");
      if(eagerLoadGroupElement != null) {
         eagerLoadGroup = MetaData.getElementContent(eagerLoadGroupElement);
      } else {
         eagerLoadGroup = defaultValue.getEagerLoadGroup();
      }
   }

   /**
    * Is read ahead strategy is none.
    */
   public boolean isNone() {
      return (strategy == NONE);
   }

   /**
    * Is the read ahead stratey on-load
    */
   public boolean isOnLoad() {
      return (strategy == ON_LOAD);
   }

   /**
    * Is the read ahead stratey on-find
    */
   public boolean isOnFind() {
      return (strategy == ON_FIND);
   }

   public boolean isDeepReadAhead()
   {
      return deepReadAhead;
   }

   /**
    * Gets the read ahead page size.
    */
   public int getPageSize() {
      return pageSize;
   }

   /**
    * Gets the eager load group.
    */
   public String getEagerLoadGroup() {
      return eagerLoadGroup;
   }

   /**
    * Returns a string describing this JDBCReadAheadMetaData.
    * @return a string representation of the object
    */
   public String toString() {
      return "[JDBCReadAheadMetaData :"+
            " strategy=" + STRATEGIES.get(strategy) +
            ", pageSize=" + pageSize +
            ", eagerLoadGroup=" + eagerLoadGroup + "]";
   }
}
