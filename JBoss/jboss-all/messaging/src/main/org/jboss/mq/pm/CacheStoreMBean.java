/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mq.pm;


import javax.management.ObjectName;

/**
 * CacheStoreMBean.java
 *
 *
 * @author <a href="mailto:d_jencks@users.sourceforge.net">david jencks</a>
 * @author <a href="mailto:cojonudo14@hotmail.com">Hiram Chirino</a>
 * @version
 */

public interface CacheStoreMBean 
{
   //methods needed for JBossMQService to set up connections
   //and to require the MessageCache.
   Object getInstance();
   
}// CacheStoreMBean
