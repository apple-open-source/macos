/*
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.test.foedeployer.ejb.simple;

import java.rmi.RemoteException;

import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.FinderException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.apache.log4j.Category;

import org.jboss.test.foedeployer.ejb.simple.SecretLocal;
import org.jboss.test.foedeployer.ejb.simple.SecretLocalHome;

/**
 * Secret Manager Session bean.
 *
 * @ejb.bean
 *    type="Stateless"
 *    name="SecretManager"
 *    jndi-name="ejb/SecretManager"
 *    view-type="remote"
 *    generate="true"
 *
 * @ejb.transaction type="Required"
 *
 * @ejb.ejb-ref
 *    ejb-name="Secret"
 *    view-type="local"
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 */
public class SecretManagerSessionBean
   implements SessionBean
{
   // Attributes ----------------------------------------------------
   private SessionContext context;
   private static Category log = Category.getInstance( SecretManagerSessionBean.class );

   // Public --------------------------------------------------------
   /**
    * Creates a new secret
    *
    * @ejb:interface-method
    */
   public void createSecret( String secretKey, String secret )
   {
      SecretLocalHome secretLocalHome = getSecretLocalHome();
      try
      {
         SecretLocal secretLocal = secretLocalHome.create( secretKey, secret );
      }
      catch( CreateException ce )
      {
         log.info("Exception creating secret with secretKey=" + secretKey, ce);
         throw new EJBException( "Exception creating secret with secretKey="
            + secretKey + ":\n" + ce );
      }
      log.info("Created secret: secretKey=" + secretKey + ", secret=" + secret);
   }

   /**
    * Removes secret
    *
    * @ejb:interface-method
    */
   public void removeSecret( String secretKey )
   {
      SecretLocalHome secretLocalHome = getSecretLocalHome();
      try
      {
         SecretLocal secretLocal = secretLocalHome.findByPrimaryKey( secretKey );
         secretLocal.remove();
      }
      catch(Exception re)
      {
         log.info("Remove(): secret with secretKey=" + secretKey + " doesn't exist");
         throw new EJBException( "Can't remove secret: secret with secretKey="
            + secretKey + " doesn't exist" );
      }
      log.info( "Removed secret: secretKey=" + secretKey );
   }

   /**
   * Returns secret
   *
   * @ejb:interface-method
   */
   public String getSecret(String secretKey)
   {
      SecretLocalHome secretLocalHome = getSecretLocalHome();
      try
      {
         SecretLocal secretLocal = secretLocalHome.findByPrimaryKey( secretKey );
         return secretLocal.getSecret();
      }
      catch( Exception re )
      {
         log.info( "getSecret(): secret with secretKey=" + secretKey + " doesn't exist" );
         throw new EJBException( "Can't find secret with secretKey=" + secretKey );
      }
   }

   // Private -------------------------------------------------------
   private SecretLocalHome getSecretLocalHome()
      throws EJBException
   {
      InitialContext initCtx = null;
      try
      {
         initCtx = new InitialContext();
         SecretLocalHome secretLocalHome = (SecretLocalHome)
            initCtx.lookup("ejb/SecretLocal");
         return secretLocalHome;
      }
      catch( NamingException ne )
      {
         log.info( "Failed to lookup SecretLocalHome." );
         throw new EJBException( ne );
      }
      finally
      {
         try
         {
            if( initCtx != null )
               initCtx.close();
         }
         catch( NamingException ne )
         {
            log.info( "Error closing context: " + ne );
            throw new EJBException( ne );
         }
      }
   }

   // SessionBean Implementation ------------------------------------
   /**
    * @ejb:create-method
    */
   public void ejbCreate() { }

   public void setSessionContext(SessionContext sc)
   {
      context = sc;
   }

   public void unsetSessionContext()
   {
      context = null;
   }

   public void ejbRemove() { }
   public void ejbActivate() { }
   public void ejbPassivate() { }
}
