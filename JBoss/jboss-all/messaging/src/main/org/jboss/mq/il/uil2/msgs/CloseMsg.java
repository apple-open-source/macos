/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.uil2.msgs;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class CloseMsg extends BaseMsg
{
   public CloseMsg()
   {
      super(MsgTypes.m_connectionClosing);
   }
}
