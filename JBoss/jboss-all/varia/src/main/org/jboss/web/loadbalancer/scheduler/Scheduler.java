/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer.scheduler;

import java.net.URL;
import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.httpclient.HttpClient;
import org.apache.commons.httpclient.HttpMethod;
import org.w3c.dom.Element;

/**
 * Interface for schedulers.
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.12.2.1 $
 */
public interface Scheduler {
  public void init(Element config) throws Exception;

  public URL getHost(HttpServletRequest request, HttpServletResponse response) throws
      NoHostAvailableException;

  public void setNodeDown(HttpServletRequest request,
                          HttpServletResponse response,
                          HttpClient client,
                          HttpMethod method
                          );
}
