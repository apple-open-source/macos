/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.xml;

/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
/**
 *  This class is for a basic unit test of the XElement class
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public class XElementTester {

   /**
    *  Starts the application.
    *
    * @param  args           an array of command-line arguments
    * @exception  Exception  Description of Exception
    */
   public static void main( java.lang.String[] args )
      throws Exception {

      XElement o = XElement.createFrom( new java.net.URL( "file:" + new java.io.File( args[0] ).getAbsolutePath() ) );
      System.out.println( "Result=" + o );

   }
}
