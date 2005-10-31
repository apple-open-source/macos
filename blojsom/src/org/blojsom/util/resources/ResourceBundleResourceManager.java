/**
 * Copyright (c) 2003-2005, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2005 by Mark Lussier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of the "David A. Czarnecki" and "blojsom" nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * Products derived from this software may not be called "blojsom",
 * nor may "blojsom" appear in their name, without prior written permission of
 * David A. Czarnecki.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package org.blojsom.util.resources;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.BlojsomException;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;

import java.text.MessageFormat;
import java.util.Locale;
import java.util.MissingResourceException;
import java.util.ResourceBundle;

/**
 * ResourceBundleResourceManager
 *
 * @author David Czarnecki
 * @version $Id: ResourceBundleResourceManager.java,v 1.2.2.1 2005/07/21 14:11:05 johnan Exp $
 * @since blojsom 2.13
 */
public class ResourceBundleResourceManager implements BlojsomConstants, ResourceManager {

    private transient Log _logger = LogFactory.getLog(ResourceBundleResourceManager.class);

    /**
     * Default constructor;
     */
    public ResourceBundleResourceManager() {
    }

    /**
     * Initialize the resource bundle manager.
     * <p/>
     * Resource bundles to pre-load are specified in a comma-separated list under the key
     * <code>blojsom-resource-manager-bundles</code>.
     *
     * @param blojsomConfiguration Blojsom configuration information
     */
    public void init(BlojsomConfiguration blojsomConfiguration) throws BlojsomException {
        String resourceBundlesToLoad = blojsomConfiguration.getBlojsomPropertyAsString(BLOJSOM_RESOURCE_MANAGER_BUNDLES_IP);
        if (!BlojsomUtils.checkNullOrBlank(resourceBundlesToLoad)) {
            String[] resourceBundles = BlojsomUtils.parseCommaList(resourceBundlesToLoad);
            if (resourceBundles != null && resourceBundles.length > 0) {
                for (int i = 0; i < resourceBundles.length; i++) {
                    String resourceBundle = resourceBundles[i];
                    try {
                        ResourceBundle.getBundle(resourceBundle);
                        _logger.debug("Loaded resource bundle: " + resourceBundle);
                    } catch (Exception e) {
                        _logger.error(e);
                    }
                }
            }
        }

        _logger.debug("Initialized resource bundle resource manager");
    }

    /**
     * Retrieve a string from a given resource bundle for the default locale.
     *
     * @param resourceID Resource ID to retrieve from the resource bundle
     * @param resource   Full-qualified resource bundle from which to retrieve the resource ID
     * @param fallback   Fallback string to use if the given resource ID cannot be found
     * @return <code>resourceID</code> from resource bundle <code>resource</code> or <code>fallback</code> if the given resource ID cannot be found
     */
    public String getString(String resourceID, String resource, String fallback) {
        try {
            ResourceBundle resourceBundle = ResourceBundle.getBundle(resource);
            return resourceBundle.getString(resourceID);
        } catch (MissingResourceException e) {
            _logger.error(e);
        }

        return fallback;
    }

    /**
     * Retrieve a string from a given resource bundle for the particular language and country locale.
     *
     * @param resourceID Resource ID to retrieve from the resource bundle
     * @param resource   Full-qualified resource bundle from which to retrieve the resource ID
     * @param fallback   Fallback string to use if the given resource ID cannot be found
     * @param language   Language code
     * @return <code>resourceID</code> from resource bundle <code>resource</code> or <code>fallback</code> if the given resource ID cannot be found
     */
    public String getString(String resourceID, String resource, String fallback, String language) {
        return getString(resourceID, resource, fallback, new Locale(language));
    }

    /**
     * Retrieve a string from a given resource bundle for the particular language and country locale.
     *
     * @param resourceID Resource ID to retrieve from the resource bundle
     * @param resource   Full-qualified resource bundle from which to retrieve the resource ID
     * @param fallback   Fallback string to use if the given resource ID cannot be found
     * @param language   Language code
     * @param country    Country code
     * @return <code>resourceID</code> from resource bundle <code>resource</code> or <code>fallback</code> if the given resource ID cannot be found
     */
    public String getString(String resourceID, String resource, String fallback, String language, String country) {
        return getString(resourceID, resource, fallback, new Locale(language, country));
    }

    /**
     * Retrieve a string from a given resource bundle for the particular language and country locale.
     *
     * @param resourceID Resource ID to retrieve from the resource bundle
     * @param resource   Full-qualified resource bundle from which to retrieve the resource ID
     * @param fallback   Fallback string to use if the given resource ID cannot be found
     * @param locale     Locale object to use when retrieving the resource bundle
     * @return <code>resourceID</code> from resource bundle <code>resource</code> or <code>fallback</code> if the given resource ID cannot be found
     */
    public String getString(String resourceID, String resource, String fallback, Locale locale) {
        try {
            ResourceBundle resourceBundle = ResourceBundle.getBundle(resource, locale);
            return resourceBundle.getString(resourceID);
        } catch (MissingResourceException e) {
            _logger.error(e);
        }

        return fallback;
    }

    /**
     * Wrapper for {@link java.text.MessageFormat#format(String, Object[])}
     *
     * @param pattern   Pattern
     * @param arguments Arguments to apply to pattern
     * @return String where {@link java.text.MessageFormat#format(String, Object[])} has been applied or <code>null</code>
     *         if there is an error applying the arguments to the pattern
     * @since blojsom 2.21
     */
    public String format(String pattern, Object[] arguments) {
        String value = null;

        try {
            value = MessageFormat.format(pattern, arguments);
        } catch (Exception e) {
            _logger.error(e);
        }

        return value;
    }
}