/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.test.cmp2.idxandusersql.ejb;

import java.util.Collection;
import javax.ejb.EntityBean;

/**
 * Test the dbindex feature
 * @author heiko.rupp@cellent.de
 * @version $Revision: 1.1.2.1 $
 * 
 * @ejb.bean name="CMR2"
 * 	type="CMP"
 * 	cmp-version="2.x"
 * 	view-type="local"
 *  primkey-field="pKey2"
 * 
 * @ejb.util generate="false"
 * 	
 * @ejb.persistence table-name = "CMR2"
 * 
 * @jboss.persistence
 * 	create-table = "true"
 * 	remove-table = "true"
 * 	
 */
public abstract class CMR2Bean implements EntityBean
{

   /**
    * We don't call them, just have them here to 
    * satisfy the cmp-engine
    * @ejb.create-method
    */
   public String ejbCreate() throws javax.ejb.CreateException
   {
      return null;
   }

   public void ejbPostCreate()
   {

   }

   /**
    * @ejb.interface-method 
    * @param pKey2
    */
   public abstract void setPKey2(String pKey2);

   /** 
    * @ejb.interface-method
    * @ejb.persistent-field 
    */
   public abstract String getPKey2();

   /**
    * This field gets a <dbindex/> that we want to
    * look up in the database to see if the index
    * was really created on the file. 
    * @ejb.interface-method
    * @ejb.persistent-field
    * @todo set the dbindex property here with a modern xdoclet*  
    */
   public abstract String getFoo2();

   /**
    * This one is not indexed 
    * @ejb.interface-method
    * @ejb.persistent-field
    */
   public abstract String getBar2();

   // 
   // many-many relation to CMR2
   // 
   /**
    * @ejb.interface-method
    */
   public abstract Collection getIdxs();

   /**
    * @ejb.interface-method
    */
   public abstract void setIdxs(Collection Idxs);

}
