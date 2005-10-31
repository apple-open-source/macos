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
package org.blojsom.plugin.showmore;

import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomProperties;

import javax.servlet.ServletConfig;
import java.io.IOException;
import java.io.InputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.util.Properties;

/**
 * Show Me More Utilities
 *
 * @author David Czarnecki
 * @version $Id: ShowMeMoreUtilities.java,v 1.1.2.1 2005/07/21 04:30:40 johnan Exp $
 * @since blojsom 2.20
 */
public class ShowMeMoreUtilities {

    /**
     * Private constructor
     */
    private ShowMeMoreUtilities() {
    }

    /**
     * Load the Show Me More configuration file
     *
     * @param blogID Blog ID
     * @param showMeMoreConfigurationFile Configuration filename
     * @param blojsomConfiguration {@link BlojsomConfiguration}
     * @param servletConfig {@link ServletConfig}
     * @return {@link ShowMeMoreConfiguration} object
     * @throws IOException If error in reading configuration file
     */
    public static ShowMeMoreConfiguration loadConfiguration(String blogID, String showMeMoreConfigurationFile, BlojsomConfiguration blojsomConfiguration, ServletConfig servletConfig) throws IOException {
        Properties showMeMoreProperties = new BlojsomProperties();
        String configurationFile = blojsomConfiguration.getBaseConfigurationDirectory() + blogID + '/' + showMeMoreConfigurationFile;
        InputStream is = servletConfig.getServletContext().getResourceAsStream(configurationFile);
        if (is == null) {
            throw new IOException("No show me more configuration file found: " + configurationFile);
        } else {
            showMeMoreProperties.load(is);
            is.close();

            String moreText = showMeMoreProperties.getProperty(ShowMeMorePlugin.SHOW_ME_MORE_TEXT);
            String textCutoff = showMeMoreProperties.getProperty(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF);
            String textCutoffStart = showMeMoreProperties.getProperty(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_START);
            String textCutoffEnd = showMeMoreProperties.getProperty(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_END);
            int cutoff;
            try {
                cutoff = Integer.parseInt(showMeMoreProperties.getProperty(ShowMeMorePlugin.ENTRY_LENGTH_CUTOFF));
            } catch (NumberFormatException e) {
                cutoff = ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_DEFAULT;
            }

            ShowMeMoreConfiguration showMeMore = new ShowMeMoreConfiguration(cutoff, textCutoff, moreText, textCutoffStart, textCutoffEnd);
            return showMeMore;
        }
    }

    /**
     * Save the Show Me More configuration to a file
     *
     * @param blogID Blog ID
     * @param showMeMoreConfigurationFile Configuration filename
     * @param blojsomConfiguration {@link BlojsomConfiguration}
     * @param showMeMoreConfiguration {@link ShowMeMoreConfiguration}
     */
    public static void saveConfiguration(String blogID, String showMeMoreConfigurationFile, BlojsomConfiguration blojsomConfiguration, ShowMeMoreConfiguration showMeMoreConfiguration) throws IOException {
        Properties showMeMoreProperties = new BlojsomProperties();
        showMeMoreProperties.put(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF, showMeMoreConfiguration.getTextCutoff());
        showMeMoreProperties.put(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_START, showMeMoreConfiguration.getTextCutoffStart());
        showMeMoreProperties.put(ShowMeMorePlugin.ENTRY_TEXT_CUTOFF_END, showMeMoreConfiguration.getTextCutoffEnd());
        showMeMoreProperties.put(ShowMeMorePlugin.SHOW_ME_MORE_TEXT, showMeMoreConfiguration.getMoreText());
        showMeMoreProperties.put(ShowMeMorePlugin.ENTRY_LENGTH_CUTOFF, Integer.toString(showMeMoreConfiguration.getCutoff()));

        File showMeMoreConfig = new File(blojsomConfiguration.getInstallationDirectory() +
                blojsomConfiguration.getBaseConfigurationDirectory() + blogID + "/" +
                showMeMoreConfigurationFile);
        FileOutputStream fos = new FileOutputStream(showMeMoreConfig);
        showMeMoreProperties.store(fos, null);
        fos.close();
    }
}