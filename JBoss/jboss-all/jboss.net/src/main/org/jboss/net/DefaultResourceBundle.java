/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

// $Id: DefaultResourceBundle.java,v 1.1 2001/10/03 13:20:31 cgjung Exp $

package org.jboss.net;

/**
 * A resource bundle that allows us to fake i18n support.
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 28. September 2001, 13:04
 * @version $Revision: 1.1 $
 */

public class DefaultResourceBundle extends java.util.ResourceBundle {

    /** empty enumeration because we translate everything! */
    static class EmptyEnumeration implements java.util.Enumeration {
        public boolean hasMoreElements() {
            return false;
        }
        
        public Object nextElement() throws java.util.NoSuchElementException {
            throw new java.util.NoSuchElementException();
        }
    }
    
    /** it´s a flyweight! */
    static java.util.Enumeration EMPTY_ENUMERATION=new EmptyEnumeration();
    
    /** Creates new DefaultResourceBundle */
    public DefaultResourceBundle() {
    }

    /** do not support this correctly */
    public java.util.Enumeration getKeys() {
        return EMPTY_ENUMERATION;
    }
    
    /** smart, isn´t it? */
    protected java.lang.Object handleGetObject(java.lang.String str) throws java.util.MissingResourceException {
        return str;
    }
    
}
