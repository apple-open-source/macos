/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.foedeployer.ejb.simple;

import javax.ejb.CreateException;
import javax.ejb.EntityBean;
import javax.ejb.EntityContext;
import javax.ejb.RemoveException;

/**
 * Models a top secret.
 *
 * @ejb.bean
 *    name="Secret"
 *    generate="true"
 *    view-type="both"
 *    type="CMP"
 *    jndi-name="ejb/Secret"
 *    local-jndi-name="ejb/SecretLocal"
 *    reentrant="False"
 *    cmp-version="2.x"
 *    primkey-field="secretKey"
 *
 * @ejb.pk
 *    class="java.lang.String"
 *    generate="false"
 *
 * @ejb.transaction type="Required"
 *
 * @@ejb.persistence table-name="SECRET"
 * @weblogic:table-name secret
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 */
public abstract class SecretBean
   implements EntityBean
{
   // Attributes ----------------------------------------------------
   private EntityContext mContext;
   
   // CMP Accessors -------------------------------------------------
   /**
    * Secret key: primary key field
    *
    * @ejb.pk-field
    * @ejb.persistent-field
    * @ejb.interface-method
    *
    * xdoclet needs to be updated
    * @@ejb.persistence
    *    column-name="username"
    *    jdbc-type="VARCHAR"
    *    sql-type="VARCHAR(32)"
    *
    * @weblogic:dbms-column secret_key
    */
   public abstract String getSecretKey();
   public abstract void setSecretKey( String secretKey );
   
   /**
    * Secret: persistent field
    *
    * @ejb.persistent-field
    * @ejb.interface-method
    *
    * xdoclet needs to be updated
    * @@ejb.persistence
    *    column-name="password"
    *
    * @weblogic:dbms-column secret
    */
   public abstract String getSecret();
   /**
    * @ejb.interface-method
    */
   public abstract void setSecret( String secret );
   
   // EntityBean Implementation -------------------------------------
   /**
    * @ejb.create-method
    */
   public String ejbCreate( String secretKey, String secret )
      throws CreateException
   {
      setSecretKey(secretKey);
      setSecret(secret);
      return null;
   }

   public void ejbPostCreate( String secretKey, String secret ) { }

   public void setEntityContext( EntityContext ctx )
   {
      mContext = ctx;
   }
   
   public void unsetEntityContext()
   {
      mContext = null;
   }
   
   public void ejbRemove() throws RemoveException { }
   public void ejbActivate() { }
   public void ejbPassivate() { }
   public void ejbLoad() { }
   public void ejbStore() { }
}
