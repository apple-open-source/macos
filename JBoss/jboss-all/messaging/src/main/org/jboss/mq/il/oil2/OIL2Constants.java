/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.il.oil2;

/**
 * 
 *
 * @author    <a href="mailto:hiram.chirino@jboss.org">Hiram Chirino</a>
 * @author    Brian Weaver (weave@opennms.org)
 * @version   $Revision: 1.2 $
 */
interface OIL2Constants
{
   // Success and error conditions are 
   // defined here
   //
   final static byte RESULT_VOID                           = 1;
   final static byte RESULT_OBJECT                         = 2;
   final static byte RESULT_EXCEPTION                      = 3;

   // The "message" codes start here.
   //
   final static byte CLIENT_RECEIVE                        = -1;
   final static byte CLIENT_DELETE_TEMPORARY_DESTINATION   = -2;
   final static byte CLIENT_CLOSE                          = -3;
   final static byte CLIENT_PONG                           = -4;
   
   final static byte SERVER_ACKNOWLEDGE                    = 1;
   final static byte SERVER_ADD_MESSAGE                    = 2;
   final static byte SERVER_BROWSE                         = 3;
   final static byte SERVER_CHECK_ID                       = 4;
   final static byte SERVER_CONNECTION_CLOSING             = 5;
   final static byte SERVER_CREATE_QUEUE                   = 6;
   final static byte SERVER_CREATE_TOPIC                   = 7;
   final static byte SERVER_DELETE_TEMPORARY_DESTINATION   = 8;
   final static byte SERVER_GET_ID                         = 9;
   final static byte SERVER_GET_TEMPORARY_QUEUE            = 10;
   final static byte SERVER_GET_TEMPORARY_TOPIC            = 11;
   final static byte SERVER_RECEIVE                        = 12;
   final static byte SERVER_SET_ENABLED                    = 13;
   final static byte SERVER_SET_SPY_DISTRIBUTED_CONNECTION = 14;
   final static byte SERVER_SUBSCRIBE                      = 15;
   final static byte SERVER_TRANSACT                       = 16;
   final static byte SERVER_UNSUBSCRIBE                    = 17;
   final static byte SERVER_DESTROY_SUBSCRIPTION           = 18;
   final static byte SERVER_CHECK_USER                     = 19;
   final static byte SERVER_PING                           = 20;
   final static byte SERVER_CLOSE                          = 21;
   final static byte SERVER_AUTHENTICATE                   = 22;
}
/*
vim:tabstop=3:expandtab:shiftwidth=3
*/
