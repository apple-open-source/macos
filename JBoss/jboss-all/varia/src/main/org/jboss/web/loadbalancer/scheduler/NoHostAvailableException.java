/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer.scheduler;

/**
 * This exception is thrown when no host is available
 * to handle a request.
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.1.2.1 $
 */
public class NoHostAvailableException
    extends Exception {
  public NoHostAvailableException() {
    super();
  }

  public NoHostAvailableException(String message) {
    super(message);
  }
}