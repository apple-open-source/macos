/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.classloader.concurrentload;

/**
 * <description>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.4.1 $
 *
 */

public class Anyclass7 extends Anyclass6
{
   public Anyclass6 circular6 = new Anyclass6();
   
   public Anyclass7 ()
   {
   }
}
