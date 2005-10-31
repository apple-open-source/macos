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
package org.blojsom.plugin.export;

import com.thoughtworks.xstream.XStream;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.fetcher.BlojsomFetcher;
import org.blojsom.fetcher.BlojsomFetcherException;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.plugin.admin.WebAdminPlugin;
import org.blojsom.util.BlojsomUtils;

import javax.servlet.ServletConfig;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/**
 * Export Blog plugin
 *
 * @author David Czarnecki
 * @version $Id: ExportBlogPlugin.java,v 1.1.2.1 2005/07/21 04:30:21 johnan Exp $
 * @since blojsom 2.17
 */
public class ExportBlogPlugin extends WebAdminPlugin {

    private Log _logger = LogFactory.getLog(ExportBlogPlugin.class);
    private BlojsomFetcher _fetcher;

    /**
     * Default constructor
     */
    public ExportBlogPlugin() {
    }

    /**
     * Return the display name for the plugin
     *
     * @return Display name for the plugin
     */
    public String getDisplayName() {
        return "Export Blog plugin";
    }

    /**
     * Return the name of the initial editing page for the plugin
     *
     * @return Name of the initial editing page for the plugin
     */
    public String getInitialPage() {
        return "";
    }

    /**
     * Initialize this plugin. This method only called when the plugin is instantiated.
     *
     * @param servletConfig        Servlet config object for the plugin to retrieve any initialization parameters
     * @param blojsomConfiguration {@link org.blojsom.blog.BlojsomConfiguration} information
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error initializing the plugin
     */
    public void init(ServletConfig servletConfig, BlojsomConfiguration blojsomConfiguration) throws BlojsomPluginException {
        super.init(servletConfig, blojsomConfiguration);

        String fetcherClassName = blojsomConfiguration.getFetcherClass();
        try {
            Class fetcherClass = Class.forName(fetcherClassName);
            _fetcher = (BlojsomFetcher) fetcherClass.newInstance();
            _fetcher.init(servletConfig, blojsomConfiguration);
            _logger.info("Added blojsom fetcher: " + fetcherClassName);
        } catch (ClassNotFoundException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (InstantiationException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (IllegalAccessException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        } catch (BlojsomFetcherException e) {
            _logger.error(e);
            throw new BlojsomPluginException(e);
        }

        _logger.debug("Initialized export entries plugin");
    }

    /**
     * Process the blog entries
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @param user                {@link org.blojsom.blog.BlogUser} instance
     * @param context             Context
     * @param entries             Blog entries retrieved for the particular request
     * @return Modified set of blog entries
     * @throws org.blojsom.plugin.BlojsomPluginException
     *          If there is an error processing the blog entries
     */
    public BlogEntry[] process(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse, BlogUser user, Map context, BlogEntry[] entries) throws BlojsomPluginException {
        entries = super.process(httpServletRequest, httpServletResponse, user, context, entries);

        String page = BlojsomUtils.getRequestValue(PAGE_PARAM, httpServletRequest);

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            Map fetchParameters = new HashMap();
            fetchParameters.put(BlojsomFetcher.FETCHER_FLAVOR, user.getBlog().getBlogDefaultFlavor());
            fetchParameters.put(BlojsomFetcher.FETCHER_NUM_POSTS_INTEGER, new Integer(-1));

            try {
                BlogEntry[] allEntries = _fetcher.fetchEntries(fetchParameters, user);
                SimpleDateFormat simpleDateFormat = new SimpleDateFormat("yyyy-MM-dd");
                String exportDate = simpleDateFormat.format(new Date());

                httpServletResponse.setContentType("application/zip");
                httpServletResponse.setHeader("Content-Disposition", "filename=blojsom-export-" + exportDate + ".zip");
                ZipOutputStream zipOutputStream = new ZipOutputStream(httpServletResponse.getOutputStream());
                zipOutputStream.putNextEntry(new ZipEntry("entries.xml"));

                OutputStreamWriter outputStreamWriter = new OutputStreamWriter(zipOutputStream, UTF8);
                XStream xStream = new XStream();
                xStream.toXML(allEntries, outputStreamWriter);
                zipOutputStream.closeEntry();

                int length;

                // Zip the resources
                File resourcesDirectory = new File(_blojsomConfiguration.getQualifiedResourceDirectory() + "/" + user.getId() + "/");
                String resourcesDirName = _blojsomConfiguration.getResourceDirectory();
                File[] resourceFiles = resourcesDirectory.listFiles();

                if (resourceFiles != null && resourceFiles.length > 0) {
                    for (int i = 0; i < resourceFiles.length; i++) {
                        File resourceFile = resourceFiles[i];
                        if (!resourceFile.isDirectory()) {
                            byte[] buffer = new byte[1024];

                            zipOutputStream.putNextEntry(new ZipEntry(resourcesDirName + user.getId() + "/" + resourceFile.getName()));
                            FileInputStream in = new FileInputStream(resourceFile.getAbsolutePath());
                            while ((length = in.read(buffer)) > 0) {
                                zipOutputStream.write(buffer, 0, length);
                            }
                            zipOutputStream.closeEntry();
                        }
                    }
                }

                // Zip the templates
                File templatesDirectory = new File(_blojsomConfiguration.getInstallationDirectory() +
                        _blojsomConfiguration.getBaseConfigurationDirectory() + user.getId() +
                        _blojsomConfiguration.getTemplatesDirectory());
                String templateDirName = _blojsomConfiguration.getTemplatesDirectory();
                File[] templateFiles = templatesDirectory.listFiles();

                if (templateFiles != null && templateFiles.length > 0) {
                    for (int i = 0; i < templateFiles.length; i++) {
                        File templateFile = templateFiles[i];
                        if (!templateFile.isDirectory()) {
                            byte[] buffer = new byte[1024];

                            zipOutputStream.putNextEntry(new ZipEntry(templateDirName + user.getId() + "/" + templateFile.getName()));
                            FileInputStream in = new FileInputStream(templateFile.getAbsolutePath());
                            while ((length = in.read(buffer)) > 0) {
                                zipOutputStream.write(buffer, 0, length);
                            }
                            zipOutputStream.closeEntry();
                        }
                    }
                }

                zipOutputStream.close();
            } catch (BlojsomFetcherException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to retrieve entries for user: " + user.getId());
            } catch (IOException e) {
                _logger.error(e);
                addOperationResultMessage(context, "Unable to create XML archive of entries for user: " + user.getId());
            }
        }

        return entries;
    }
}