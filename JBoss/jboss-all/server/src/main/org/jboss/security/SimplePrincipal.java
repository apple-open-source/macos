/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.security;

import java.security.Principal;

/** A simple String based implementation of Principal. Typically
a SimplePrincipal is created given a userID which is used
as the Principal name.

@author <a href="on@ibis.odessa.ua">Oleg Nitz</a>
@author Scott.Stark@jboss.org
*/
public class SimplePrincipal implements Principal, java.io.Serializable
{
  private String name;

  public SimplePrincipal(String name)
  {
    this.name = name;
  }

  /** Compare this SimplePrincipal's name against another Principal
  @return true if name equals another.getName();
   */
  public boolean equals(Object another)
  {
    if( !(another instanceof Principal) )
      return false;
    String anotherName = ((Principal)another).getName();
    boolean equals = false;
    if( name == null )
      equals = anotherName == null;
    else
      equals = name.equals(anotherName);
    return equals;
  }

  public int hashCode()
  {
    return (name == null ? 0 : name.hashCode());
  }

  public String toString()
  {
    return name;
  }

  public String getName()
  {
    return name;
  }
} 
