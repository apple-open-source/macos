/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test;

import junit.framework.Test;

/**
 * Derived implementation of JBossTestSetup for cluster testing.
 *
 * @see org.jboss.test.JBossTestSetup
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>12 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class JBossTestClusteredSetup extends JBossTestSetup
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public JBossTestClusteredSetup(Test test) throws Exception
   {
      super(test);
   }
   
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected JBossTestServices createTestServices()
   {
      return new JBossTestClusteredServices(getClass().getName());
   }

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
