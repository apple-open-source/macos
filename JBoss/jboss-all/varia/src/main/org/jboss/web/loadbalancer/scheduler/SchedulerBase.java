/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer.scheduler;

import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Iterator;
import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.apache.commons.httpclient.HttpClient;
import org.apache.commons.httpclient.HttpMethod;
import org.jboss.logging.Logger;
import org.jboss.metadata.MetaData;

import org.w3c.dom.Element;
import java.util.Collection;

/**
 * Base-class for Scheduler
 *
 * @jmx:mbean name="jboss.web.loadbalancer: service=Scheduler"
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.3.2.1 $
 */
public abstract class SchedulerBase
    implements Scheduler, SchedulerBaseMBean {
  protected Logger log = Logger.getLogger(this.getClass());
  protected ArrayList hostsUp = new ArrayList();
  protected ArrayList hostsDown = new ArrayList();
  protected String stickyCookieName;
  protected boolean useStickySession = false;

  public void init(Element config) throws Exception {
    // Add all hosts to the host-up-list
    addHostsFromConfig(config, hostsUp);

    // do we use sticky session?
    if (MetaData.getUniqueChildContent(config,"sticky-session").equalsIgnoreCase("true"))
    {
      useStickySession = true;
      stickyCookieName = MetaData.getUniqueChildContent(config,"sticky-session-cookie-name");
    }

    if (log.isDebugEnabled()) {
      if (useStickySession) {
        log.debug("Using sticky sessions with Cookie name=" + stickyCookieName);
      }
      else {
        log.debug("Using NO sticky sessions");
      }
    }
  }

  public void addHostsFromConfig(Element config, Collection hosts) throws Exception {
    Iterator hostIterator=MetaData.getChildrenByTagName(MetaData.getUniqueChild(config, "hosts"),"host");

    while (hostIterator.hasNext()) {
      Element host=(Element)hostIterator.next();
      String hostUrl=MetaData.getUniqueChildContent(host,"host-url");
      if (!hostUrl.endsWith("/")) {
        hostUrl+="/";
      }
      hosts.add(new URL(hostUrl));
    }
  }

  public URL getHost(HttpServletRequest request, HttpServletResponse response) throws
      NoHostAvailableException {
    URL host = null;

    // first look if we can find the sticky host for this request
    host = findStickyHost(request);

    // not found -> find a host for this request
    while (host == null) {
      if (hostsUp.size() == 0) {
        throw new NoHostAvailableException("No host to schedule request");
      }
      host = getNextHost();
    }

    // if we use sticky session -> set the cookie
    if (useStickySession) {
      setStickyCookie(host, request, response);
    }

    return host;
  }

  protected abstract URL getNextHost();

  protected void setStickyCookie(URL host, HttpServletRequest request,
                                 HttpServletResponse response) {
    Cookie cookie = new Cookie(stickyCookieName, Integer.toString(host.hashCode()));
    cookie.setPath("/");
    cookie.setMaxAge( -1);
    response.addCookie(cookie);
  }

  /**
   * Find the sticky host for the given request
   * @param request The request we want to find the sticky host for
   * @return null=host not found, otherwise the sticky host URL for this request
   */
  protected URL findStickyHost(HttpServletRequest request) {
    URL host = null;
    if (useStickySession) {
      Cookie[] cookies = request.getCookies();

      for (int i = 0; cookies != null && i < cookies.length; ++i) {
        Cookie cookie = cookies[i];

        if (cookie.getName().equals(stickyCookieName)) {
          log.debug("Sticky Cookie found!");
          int cookieHash=Integer.parseInt(cookie.getValue());
          Iterator iter=hostsUp.iterator();

          while (iter.hasNext()) {
            URL url=(URL)iter.next();
            if (url.hashCode()==cookieHash) {
              host=url;
              if (log.isDebugEnabled()) {
                log.debug("Sticky Cookie sticks client to host with URL " +
                          url.toExternalForm());
              }
              break;
            }
          }
          break;
        }
      }
      return host;
    }
    else {
      return null;
    }
  }

  public void setNodeDown(HttpServletRequest request,
                          HttpServletResponse response,
                          HttpClient client,
                          HttpMethod method
                          )
  {
    log.warn("Marking host with URL "+method.getHostConfiguration().getHostURL()+"/ as DOWN");
    try {
      URL methodURL = new URL(method.getHostConfiguration().getHostURL()+"/");
      synchronized (hostsUp) {
        int index=hostsUp.indexOf(methodURL);
        if (index==-1) {
          return;
        }
        hostsUp.remove(index);
      }
      synchronized (hostsDown) {
        hostsDown.add(methodURL);
      }
    }
    catch (MalformedURLException ex) {
      log.error("Could not mark host "+method.getHostConfiguration().getHostURL()+" down",ex);
    }
  }

  // MBean Interface
  /**
   * Get the list of all hosts that have been marked down.
   * @jmx:managed-attribute
   */
  public ArrayList getHostsDown() {
    return hostsDown;
  }

  /**
   * Get the list of all hosts that have been marked up.
   * @jmx:managed-attribute
   */
  public ArrayList getHostsUp() {
    return hostsUp;
  }

  /**
   * Add a host to the up list.
   * @jmx:managed-operation
   */
  public void addHost(String host) throws MalformedURLException
  {
    if (host==null) {
      return;
    }
    if (!host.endsWith("/"))
    {
      host+="/";
    }
    synchronized (hostsUp) {
      hostsUp.add(new URL(host));
    }
  }

  /**
   * Remove a host from the up list.
   * @jmx:managed-operation
   */
  public void removeHost(URL host)
  {
    if (host==null) {
      return;
    }

    synchronized (hostsUp) {
      int index = hostsUp.indexOf(host);
      if (index==-1) {
        return;
      }
      hostsUp.remove(index);
    }
  }

  /**
   * Remove all hosts from the up list.
   * @jmx:managed-operation
   */
  public void removeAllHosts()
  {
    synchronized (hostsUp) {
      hostsUp.clear();
    }
  }

  /**
   * Clear the hosts down list.
   * @jmx:managed-operation
   */
  public void clearHostsDown()
  {
    synchronized (hostsDown) {
      hostsDown.clear();
    }
  }
}
