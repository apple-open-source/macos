/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.monitor;

/**
 * Metrics constants interface contains JMS message types
 * used to identify different monitoring point
 * message producers in the server. When publishing a message
 * to the metrics topic, you should type the message source.
 * For example:  <br><pre>
 *
 *      Message myMessage;
 *      myMessage.setJMSType(INVOCATION_METRICS);
 *
 * </pre>
 *
 * In addition, this interface contains some generic JMS property
 * identifiers for the metrics messages.
 *
 * @author  <a href="mailto:jplindfo@helsinki.fi">Juha Lindfors</a>
 * @version $Revision: 1.4 $
 */     
public interface MetricsConstants {
    
	// Constants ----------------------------------------------------
    /** Method invocation metrics producer. */
    final static String INVOCATION_METRICS = "Invocation";
    /** Bean cache metrics producer. */
    final static String BEANCACHE_METRICS  = "BeanCache";
    /** System resource metrics producer. */
    final static String SYSTEM_METRICS     = "System";
    

    /** Message property 'TIME' */
    final static String TIME        = "TIME";
    /** Message property 'APPLICATION' */
    final static String APPLICATION = "APPLICATION";
    /** Message property 'BEAN' */
    final static String BEAN        = "BEAN";
    /** Message propertu 'PRIMARY_KEY' */
    final static String PRIMARY_KEY = "PRIMARY_KEY";
    /** Message property 'TYPE' */
    final static String TYPE        = "TYPE";
    /** Message property 'ACTIVITY' */
    final static String ACTIVITY    = "ACTIVITY";
    /** Message property 'CHECKPOINT' */
    final static String CHECKPOINT  = "CHECKPOINT";
    /** Message property 'METHOD' */
    final static String METHOD      = "METHOD";
   
   
    /** System Monitor TYPE */
    final static String THREAD_MONITOR = "ThreadMonitor";
    /** System Monitor TYPE */
    final static String MEMORY_MONITOR = "MemoryMonitor";
    
}


