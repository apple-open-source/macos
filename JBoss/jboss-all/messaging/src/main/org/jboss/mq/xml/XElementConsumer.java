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
 *  XMLRecordConsumer Interface defines the method signatures used to notify the
 *  consumer object of parsing errors, document starts, record reads, and
 *  document ends.
 *
 * @author     Hiram Chirino (Cojonudo14@hotmail.com)
 * @created    August 16, 2001
 * @version    $Revision: 1.2 $
 */
public interface XElementConsumer {

   /**
    *  Signals that the END of the XML document has been reached.
    *
    * @exception  Exception  Description of Exception
    */
   public void documentEndEvent()
      throws Exception;

   /**
    *  Signals that the START of the XML document has been reached.
    *
    * @exception  Exception  Description of Exception
    */
   public void documentStartEvent()
      throws Exception;

   /**
    *  Signals that a record object, an xml element, has been fully read in.
    *
    * @param  o              Description of Parameter
    * @exception  Exception  Description of Exception
    */
   public void recordReadEvent( XElement o )
      throws Exception;

}
