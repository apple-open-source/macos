/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins.cmp.jdbc.keygen;

import javax.ejb.CreateException;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMPFieldBridge;
import org.jboss.ejb.plugins.cmp.jdbc.metadata.JDBCEntityCommandMetaData;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCInsertPKCreateCommand;
import org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager;
import org.jboss.ejb.plugins.keygenerator.KeyGenerator;
import org.jboss.ejb.plugins.keygenerator.KeyGeneratorFactory;

/**
 * JDBCKeyGeneratorCreateCommand executes an INSERT INTO query.
 * This command will ask the corresponding key generator for a
 * value for the primary key before inserting the row.
 *
 * @author <a href="mailto:loubyansky@hotmail.com">Alex Loubyansky</a>
 *
 * @version $Revision: 1.1.2.2 $
 */
public class JDBCKeyGeneratorCreateCommand extends JDBCInsertPKCreateCommand
{
   private KeyGenerator keyGenerator;
   private JDBCCMPFieldBridge pkField;

   public void init(JDBCStoreManager manager) throws DeploymentException
   {
      super.init(manager);
      pkField = getGeneratedPKField();
   }

   protected void initEntityCommand(JDBCEntityCommandMetaData entityCommand) throws DeploymentException
   {
      super.initEntityCommand(entityCommand);

      String factoryName = entityCommand.getAttribute("key-generator-factory");
      if (factoryName == null) {
         throw new DeploymentException("key-generator-factory attribute must be set for entity " + entity.getEntityName());
      }

      try {
         KeyGeneratorFactory keyGeneratorFactory = (KeyGeneratorFactory) new InitialContext().lookup(factoryName);
         keyGenerator = keyGeneratorFactory.getKeyGenerator();
      } catch (NamingException e) {
         throw new DeploymentException("Error: can't find key generator factory: " + factoryName, e);
      } catch (Exception e) {
         throw new DeploymentException("Error: can't create key generator instance; key generator factory: " + factoryName, e);
      }
   }

   protected void generateFields(EntityEnterpriseContext ctx) throws CreateException
   {
      super.generateFields(ctx);

      Object pk = keyGenerator.generateKey();
      log.debug("Generated new pk: " + pk);
      pkField.setInstanceValue(ctx, pk);
   }
}
