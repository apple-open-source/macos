/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.jaws.metadata;

import org.w3c.dom.Element;

import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.XmlLoadable;


/**
 * <description>
 *
 * @see <related>
 * @author <a href="sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="danch@nvisia.com">danch</a>
 * @author <a href="bill@burkecentral.com">Bill Burke</a>
 * @version $Revision: 1.7 $
 *
 *      Revisions:
 *      20010621 Bill Burke: setReadAhead added.
 *
 */
public class FinderMetaData
   extends MetaData
   implements XmlLoadable
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   private String name;
   private String order;
   private String query;

   /** do we perform 'read-ahead' of column values? (avoid making n+1 database hits)  */
   private boolean readAhead = false;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /** default constructor */
   public FinderMetaData() {}

   /**
    * constructor used to provide non-defined finders (findAll, BMP style
    * finders) with their metadata.
    */
   public FinderMetaData(String name)
   {
      this.name = name;
   }

   // Public --------------------------------------------------------

   public String getName() { return name; }

   public String getOrder() { return order; }

   public String getQuery() { return query; }

   public boolean hasReadAhead() { return readAhead; }

   public void setReadAhead(boolean newval)
   {
      readAhead = newval;
   }

   // XmlLoadable implementation ------------------------------------

   public void importXml(Element element)
      throws DeploymentException
   {
      name = getElementContent(getUniqueChild(element, "name"));
      query = getElementContent(getUniqueChild(element, "query"));
      order = getElementContent(getUniqueChild(element, "order"));

      // read ahead?  If not provided, keep default.
      String readAheadStr = getElementContent(getOptionalChild(element, "read-ahead"));
      if (readAheadStr != null) readAhead = Boolean.valueOf(readAheadStr).booleanValue();
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
