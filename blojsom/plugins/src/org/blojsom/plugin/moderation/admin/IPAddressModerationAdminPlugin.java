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
package org.blojsom.plugin.moderation.admin;

import org.blojsom.plugin.admin.WebAdminPlugin;
import org.blojsom.plugin.BlojsomPluginException;
import org.blojsom.blog.BlogEntry;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfiguration;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomConstants;
import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.ServletConfig;
import java.util.*;
import java.io.*;

/**
 * IP address moderation administration plugin
 *
 * @author David Czarnecki
 * @version $Id: IPAddressModerationAdminPlugin.java,v 1.1.2.1 2005/07/21 04:30:36 johnan Exp $
 * @since blojsom 2.25
 */
public class IPAddressModerationAdminPlugin extends WebAdminPlugin {

    private Log _logger = LogFactory.getLog(IPAddressModerationAdminPlugin.class);

    private static final String BLACKLIST = "blacklist";
    private static final String IP_BLACKLIST_IP = "ip-blacklist";
    private static final String IP_WHITELIST_IP = "ip-whitelist";
    private static final String DEFAULT_IP_BLACKLIST_FILE = "ip-blacklist.properties";
    private static final String DEFAULT_IP_WHITELIST_FILE = "ip-whitelist.properties";
    private String _ipBlacklist = DEFAULT_IP_BLACKLIST_FILE;
    private String _ipWhitelist = DEFAULT_IP_WHITELIST_FILE;

    // Context
    private static final String BLOJSOM_PLUGIN_IP_WHITELIST = "BLOJSOM_PLUGIN_IP_WHITELIST";
    private static final String BLOJSOM_PLUGIN_IP_BLACKLIST = "BLOJSOM_PLUGIN_IP_BLACKLIST";

    // Pages
    private static final String EDIT_IP_MODERATION_SETTINGS_PAGE = "/org/blojsom/plugin/moderation/admin/templates/admin-edit-ip-moderation-settings";

    // Form itmes
    private static final String IP_ADDRESS = "ip-address";
    private static final String LIST_TYPE = "list-type";

    // Actions
    private static final String ADD_IP_ADDRESS_ACTION = "add-ip-address";
    private static final String DELETE_IP_ADDRESS_ACTION = "delete-ip-address";

    // Permissions
    private static final String IP_MODERATION_PERMISSION = "ip_moderation";

    /**
     * Create a new instance of the IP address moderation administration plugin
     */
    public IPAddressModerationAdminPlugin() {
    }

    /**
     * Return the display name for the plugin
     *
     * @return Display name for the plugin
     */
    public String getDisplayName() {
        return "IP Address Moderation plugin";
    }

    /**
     * Return the name of the initial editing page for the plugin
     *
     * @return Name of the initial editing page for the plugin
     */
    public String getInitialPage() {
        return EDIT_IP_MODERATION_SETTINGS_PAGE;
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

        String ipBlacklist = servletConfig.getInitParameter(IP_BLACKLIST_IP);
        if (!BlojsomUtils.checkNullOrBlank(ipBlacklist)) {
            _ipBlacklist = ipBlacklist;
        }

        String ipWhitelist = servletConfig.getInitParameter(IP_WHITELIST_IP);
        if (!BlojsomUtils.checkNullOrBlank(ipWhitelist)) {
            _ipWhitelist = ipWhitelist;
        }

        _logger.debug("Initialized IP address blacklist/whitelist moderation plugin");
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

        String username = getUsernameFromSession(httpServletRequest, user.getBlog());
        if (!checkPermission(user, null, username, IP_MODERATION_PERMISSION)) {
            httpServletRequest.setAttribute(PAGE_PARAM, ADMIN_LOGIN_PAGE);
            addOperationResultMessage(context, "You are not allowed to edit IP address moderation settings");

            return entries;
        }

        if (ADMIN_LOGIN_PAGE.equals(page)) {
            return entries;
        } else {
            String action = BlojsomUtils.getRequestValue(ACTION_PARAM, httpServletRequest);
            List ipAddressesFromBlacklist = loadIPList(user, _ipBlacklist);
            List ipAddressesFromWhitelist = loadIPList(user, _ipWhitelist);
            String listType = BlojsomUtils.getRequestValue(LIST_TYPE, httpServletRequest);

            if (ADD_IP_ADDRESS_ACTION.equals(action)) {
                String ipAddress = BlojsomUtils.getRequestValue(IP_ADDRESS, httpServletRequest);

                if (BLACKLIST.equals(listType)) {
                    if (!ipAddressesFromBlacklist.contains(ipAddress)) {
                        ipAddressesFromBlacklist.add(ipAddress);
                        writeIPList(user, ipAddressesFromBlacklist, _ipBlacklist);
                        addOperationResultMessage(context, "Added IP address " + ipAddress + " to blacklist");
                    } else {
                        addOperationResultMessage(context, "IP address " + ipAddress + " has already been added to blacklist");
                    }
                } else {
                    if (!ipAddressesFromWhitelist.contains(ipAddress)) {
                        ipAddressesFromWhitelist.add(ipAddress);
                        writeIPList(user, ipAddressesFromWhitelist, _ipWhitelist);
                        addOperationResultMessage(context, "Added IP address " + ipAddress + " to whitelist");
                    } else {
                        addOperationResultMessage(context, "IP address " + ipAddress + " has already been added to whitelist");
                    }
                }
            } else if (DELETE_IP_ADDRESS_ACTION.equals(action)) {
                String[] ipAddressesToDelete = httpServletRequest.getParameterValues(IP_ADDRESS);

                if (ipAddressesToDelete != null && ipAddressesToDelete.length > 0) {
                    if (BLACKLIST.equals(listType)) {
                        for (int i = 0; i < ipAddressesToDelete.length; i++) {
                            ipAddressesFromBlacklist.set(Integer.parseInt(ipAddressesToDelete[i]), null);
                        }

                        ipAddressesFromBlacklist = BlojsomUtils.removeNullValues(ipAddressesFromBlacklist);
                        writeIPList(user, ipAddressesFromBlacklist, _ipBlacklist);
                        addOperationResultMessage(context, "Deleted " + ipAddressesToDelete.length + " IP addresses");
                    } else {
                        for (int i = 0; i < ipAddressesToDelete.length; i++) {
                            ipAddressesFromWhitelist.set(Integer.parseInt(ipAddressesToDelete[i]), null);
                        }

                        ipAddressesFromWhitelist = BlojsomUtils.removeNullValues(ipAddressesFromWhitelist);
                        writeIPList(user, ipAddressesFromWhitelist, _ipWhitelist);
                        addOperationResultMessage(context, "Deleted " + ipAddressesToDelete.length + " IP addresses");
                    }
                } else {
                    addOperationResultMessage(context, "No IP addresses selected for deletion");
                }
            }

            context.put(BLOJSOM_PLUGIN_IP_BLACKLIST, ipAddressesFromBlacklist);
            context.put(BLOJSOM_PLUGIN_IP_WHITELIST, ipAddressesFromWhitelist);
        }

        return entries;
    }

    /**
     * Load the list of IP addresses from whitelist or blacklist from the blog's configuration directory
     *
     * @param blogUser {@link BlogUser}
     * @return List of IP addresses
     */
    protected List loadIPList(BlogUser blogUser, String filename) {
        File ipAddressFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + "/" +
                blogUser.getId() + "/" + filename);
        ArrayList ipAddresses = new ArrayList(25);

        if (ipAddressFile.exists()) {
            try {
                FileInputStream fis = new FileInputStream(ipAddressFile);
                BufferedReader br = new BufferedReader(new InputStreamReader(fis, BlojsomConstants.UTF8));
                String ipAddress;

                while ((ipAddress = br.readLine()) != null) {
                    ipAddresses.add(ipAddress);
                }

                br.close();
            } catch (IOException e) {
                _logger.error(e);
            }
        }

        return ipAddresses;
    }

    /**
     * Write out the IP addresses whitelist or blacklist to the blog's configuration directory
     *
     * @param blogUser    {@link BlogUser}
     * @param ipAddresses List of IP addresses
     */
    protected void writeIPList(BlogUser blogUser, List ipAddresses, String filename) {
        File ipAddressFile = new File(_blojsomConfiguration.getInstallationDirectory() + _blojsomConfiguration.getBaseConfigurationDirectory() + "/" +
                blogUser.getId() + "/" + filename);

        try {
            FileOutputStream fos = new FileOutputStream(ipAddressFile);
            BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(fos, BlojsomConstants.UTF8));

            Iterator ipIterator = ipAddresses.iterator();
            while (ipIterator.hasNext()) {
                String ipAddress = (String) ipIterator.next();
                bw.write(ipAddress);
                bw.newLine();
            }

            bw.close();
        } catch (IOException e) {
            _logger.error(e);
        }
    }
}