/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.entityexc.interfaces;

/**
 *  Application exception for entity exception test bean.
 *
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.1 $
 */
public class MyAppException extends Exception
{
   public MyAppException(String s)
   {
      super(s);
   }
}
