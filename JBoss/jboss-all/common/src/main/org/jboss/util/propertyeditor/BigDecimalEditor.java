/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.propertyeditor;

import java.math.BigDecimal;

/**
 * A property editor for {@link java.math.BigDecimal}.
 *
 * @version <tt>$Revision: 1.2.2.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author  <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 */
public class BigDecimalEditor
   extends TextPropertyEditorSupport
{
   /**
    * Returns a BigDecimal for the input object converted to a string.
    *
    * @return a BigDecimal object
    *
    */
   public Object getValue()
   {
      return new BigDecimal(getAsText());
   }
}
