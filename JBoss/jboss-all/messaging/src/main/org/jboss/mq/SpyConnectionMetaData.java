/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq;
import java.util.Enumeration;
import java.util.Vector;

import javax.jms.ConnectionMetaData;
import javax.jms.JMSException;

/**
 *  This class implements javax.jms.ConnectionMetaData
 *
 * @author     Norbert Lataille (Norbert.Lataille@m4x.org)
 * @author     Hiram Chirino (Norbert.Lataille@m4x.org)
 * @created    August 16, 2001
 * @version    $Revision: 1.3 $
 */
public class SpyConnectionMetaData
       implements ConnectionMetaData {

   // Public --------------------------------------------------------

   public String getJMSVersion() {
      return "1.0.2";
   }

   public int getJMSMajorVersion() {
      return 1;
   }

   public int getJMSMinorVersion() {
      return 0;
   }

   public String getJMSProviderName() {
      return "JBossMQ";
   }

   public String getProviderVersion() {
      return "1.0.0 Beta";
   }

   public int getProviderMajorVersion() {
      return 1;
   }

   public int getProviderMinorVersion() {
      return 0;
   }

   public Enumeration getJMSXPropertyNames() {
      Vector vector = new Vector();
      vector.add( "JMSXGroupID" );
      vector.add( "JMSXGroupSeq" );
      return vector.elements();
   }
}
