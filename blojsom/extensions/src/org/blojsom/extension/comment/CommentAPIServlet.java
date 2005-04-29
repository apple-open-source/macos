/**
 * Copyright (c) 2003-2004, David A. Czarnecki
 * All rights reserved.
 *
 * Portions Copyright (c) 2003-2004 by Mark Lussier
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
package org.blojsom.extension.comment;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.blojsom.blog.Blog;
import org.blojsom.blog.BlogComment;
import org.blojsom.blog.BlogUser;
import org.blojsom.blog.BlojsomConfigurationException;
import org.blojsom.plugin.comment.CommentUtils;
import org.blojsom.plugin.email.EmailMessage;
import org.blojsom.plugin.email.EmailUtils;
import org.blojsom.plugin.email.SendEmailPlugin;
import org.blojsom.servlet.BlojsomBaseServlet;
import org.blojsom.util.BlojsomConstants;
import org.blojsom.util.BlojsomUtils;
import org.blojsom.util.BlojsomProperties;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

import javax.mail.Session;
import javax.mail.internet.AddressException;
import javax.mail.internet.InternetAddress;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.FactoryConfigurationError;
import javax.xml.parsers.ParserConfigurationException;
import java.io.*;
import java.util.Date;
import java.util.Properties;

/**
 * blojsom Comment API Implementation
 * <p/>
 * Comment API Specification can be found at <a href="http://wellformedweb.org/story/9">http://wellformedweb.org/story/9</a>
 * <p/>
 * For info on the <item/> fragment and it's content, thehe RSS 2.0 Spec can
 * be found at http://backend.userland.com/rss
 *
 * @author Mark Lussier
 * @author David Czarnecki
 * @version $Id: CommentAPIServlet.java,v 1.2 2004/08/27 00:49:41 whitmore Exp $
 */
public class CommentAPIServlet extends BlojsomBaseServlet implements BlojsomConstants {

    /**
     * RSS <item/> fragment tag containing the Title
     */
    private static final String COMMENTAPI_TITLE = "title";

    /**
     * RSS <item/> fragment tag containing the Link
     */
    private static final String COMMENTAPI_LINK = "link";

    /**
     * RSS <item/> fragment tag containing the Description
     */
    private static final String COMMENTAPI_DESCRIPTION = "description";

    /**
     * RSS <item/> fragment tag containing the Author
     */
    private static final String COMMENTAPI_AUTHOR = "author";

    private Log _logger = LogFactory.getLog(CommentAPIServlet.class);
    private Session _mailsession = null;
    private ServletConfig _servletConfig;

    /**
     * Default constructor
     */
    public CommentAPIServlet() {
    }

    /**
     * Initialize the blojsom Comment API servlet
     *
     * @param servletConfig Servlet configuration information
     * @throws ServletException If there is an error initializing the servlet
     */
    public void init(ServletConfig servletConfig) throws ServletException {
        super.init(servletConfig);
        _servletConfig = servletConfig;

        String _hostname = servletConfig.getInitParameter(SendEmailPlugin.SMTPSERVER_IP);
        if (_hostname != null) {
            Properties _props = new Properties();
            _props.put(SendEmailPlugin.SESSION_NAME, _hostname);
            _mailsession = Session.getInstance(_props);
        }

        configureBlojsom(servletConfig);
    }

    /**
     * Loads a {@link BlogUser} object for a given user ID
     *
     * @param userID User ID
     * @return {@link BlogUser} configured for the given user ID or <code>null</code> if there is an error loading the user
     * @since blojsom 2.16
     */
    protected BlogUser loadBlogUser(String userID) {
        BlogUser blogUser = new BlogUser();
        blogUser.setId(userID);

        try {
            Properties userProperties = new BlojsomProperties();
            InputStream is = _servletConfig.getServletContext().getResourceAsStream(_baseConfigurationDirectory + userID + '/' + BLOG_DEFAULT_PROPERTIES);

            userProperties.load(is);
            is.close();
            Blog userBlog = null;

            userBlog = new Blog(userProperties);
            blogUser.setBlog(userBlog);

            _logger.debug("Configured blojsom user: " + blogUser.getId());
        } catch (BlojsomConfigurationException e) {
            _logger.error(e);
            return null;
        } catch (IOException e) {
            _logger.error(e);
            return null;
        }

        return blogUser;
    }

    /**
     * Service a Comment API request
     *
     * @param httpServletRequest  Request
     * @param httpServletResponse Response
     * @throws ServletException If there is an error processing the request
     * @throws IOException      If there is an error during I/O
     */
    protected void service(HttpServletRequest httpServletRequest, HttpServletResponse httpServletResponse) throws ServletException, IOException {
        try {
            httpServletRequest.setCharacterEncoding(UTF8);
        } catch (UnsupportedEncodingException e) {
            _logger.error(e);
        }

        String commentAuthor = null;
        String commentEmail = null;
        String commentLink = null;
        String commentText = null;
        String commentTitle = null;

        // Determine the appropriate user from the URL
        String user;
        String userFromPath = BlojsomUtils.getUserFromPath(httpServletRequest.getPathInfo());
        String requestedCategory;

        if (userFromPath == null) {
            httpServletResponse.sendError(HttpServletResponse.SC_NOT_FOUND, "Requested user not available in URL");
            return;
        } else {
            user = userFromPath;
            requestedCategory = BlojsomUtils.getCategoryFromPath(httpServletRequest.getPathInfo());
            requestedCategory = BlojsomUtils.urlDecode(requestedCategory);
            requestedCategory = BlojsomUtils.normalize(requestedCategory);
        }

        if (BlojsomUtils.checkNullOrBlank(user) || "/".equals(user)) {
            httpServletResponse.sendError(HttpServletResponse.SC_NOT_FOUND, "Requested user not available in URL");
            return;
        }

        // Fetch the user and their blog
        BlogUser blogUser = loadBlogUser(user);
        if (blogUser == null) {
            httpServletResponse.sendError(HttpServletResponse.SC_NOT_FOUND, "Requested user not available: " + user);

            return;
        }

        Blog blog = blogUser.getBlog();

        _logger.info("Processing a comment for [" + user + "] in category [" + requestedCategory + "]");

        if (blog.getBlogCommentsEnabled().booleanValue() && httpServletRequest.getContentLength() > 0) {
            String permalink = httpServletRequest.getParameter(PERMALINK_PARAM);

            try {
                DocumentBuilder builder = DocumentBuilderFactory.newInstance().newDocumentBuilder();
                Document document = builder.parse(httpServletRequest.getInputStream());

                // Walk through the RSS2 Item Fragment
                Element docElement = document.getDocumentElement();
                if (docElement.hasChildNodes()) {
                    NodeList comment = docElement.getChildNodes();
                    if (comment.getLength() > 0) {
                        for (int x = 0; x < comment.getLength(); x++) {
                            Node node = comment.item(x);
                            if (node.getNodeType() == Node.ELEMENT_NODE) {
                                if (node.getNodeName().equals(COMMENTAPI_LINK)) {
                                    commentLink = node.getFirstChild().getNodeValue();
                                }
                                if (node.getNodeName().equals(COMMENTAPI_TITLE)) {
                                    commentTitle = node.getFirstChild().getNodeValue();
                                }
                                if (node.getNodeName().equals(COMMENTAPI_AUTHOR)) {
                                    commentAuthor = node.getFirstChild().getNodeValue();
                                }
                                if (node.getNodeName().equals(COMMENTAPI_DESCRIPTION)) {
                                    commentText = node.getFirstChild().getNodeValue();
                                }
                            }
                        }
                    }
                }
            } catch (ParserConfigurationException e) {
                _logger.error(e);
                httpServletResponse.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, e.getMessage());

                return;
            } catch (FactoryConfigurationError e) {
                _logger.error(e);
                httpServletResponse.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, e.getMessage());

                return;
            } catch (SAXException e) {
                _logger.error(e);
                httpServletResponse.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, e.getMessage());

                return;
            } catch (IOException e) {
                _logger.error(e);
                httpServletResponse.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, e.getMessage());

                return;
            }

            // Try to extract an email address from "User Name <useremail@.com>" formatted string,
            // otherwise, just use the Name
            if (commentAuthor != null) {
                try {
                    InternetAddress emailaddress = new InternetAddress(commentAuthor);
                    commentEmail = emailaddress.getAddress();
                    commentAuthor = emailaddress.getPersonal();
                } catch (AddressException e) {
                    _logger.error(e);
                }
            } else {
                commentAuthor = "";
                commentEmail = "";
            }

            // If the link is null, set it to an empty string
            if (commentLink == null) {
                commentLink = "";
            }

            if (commentText != null) {
                _logger.debug("Comment API ==============================================");
                _logger.debug(" Blog User: " + user);
                _logger.debug("  Category: " + requestedCategory);
                _logger.debug(" Permalink: " + permalink);
                _logger.debug(" Commenter: " + commentAuthor);
                _logger.debug("Cmtr Email: " + commentEmail);
                _logger.debug("      Link: " + commentLink);
                _logger.debug("   Comment: \n" + commentText);

                // Create a new blog comment
                BlogComment comment = new BlogComment();
                comment.setAuthor(commentAuthor);
                comment.setAuthorEmail(commentEmail);
                comment.setAuthorURL(commentLink);
                comment.setComment(commentText);
                comment.setCommentDate(new Date());

                // Construct the comment filename
                String permalinkFilename = BlojsomUtils.getFilenameForPermalink(permalink, blog.getBlogFileExtensions());
                StringBuffer commentDirectory = new StringBuffer(blog.getBlogHome());
                commentDirectory.append(BlojsomUtils.removeInitialSlash(requestedCategory));
                commentDirectory.append(blog.getBlogCommentsDirectory());
                commentDirectory.append(File.separator);
                commentDirectory.append(permalinkFilename);
                commentDirectory.append(File.separator);
                String hashedComment = BlojsomUtils.digestString(commentText).toUpperCase();
                String commentFilename = commentDirectory.toString() + hashedComment + BlojsomConstants.COMMENT_EXTENSION;
                File commentDir = new File(commentDirectory.toString());

                // Ensure it exists
                if (!commentDir.exists()) {
                    if (!commentDir.mkdirs()) {
                        _logger.error("Could not create directory for comments: " + commentDirectory);
                        httpServletResponse.sendError(HttpServletResponse.SC_NOT_FOUND, "Could not create directory for comments");
                    }
                }

                // Create a comment entry
                File commentEntry = new File(commentFilename);
                if (!commentEntry.exists()) {
                    try {
                        BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(commentEntry), blog.getBlogFileEncoding()));
                        bw.write(comment.getAuthor());
                        bw.newLine();
                        bw.write(comment.getAuthorEmail());
                        bw.newLine();
                        bw.write(comment.getAuthorURL());
                        bw.newLine();
                        bw.write(comment.getComment());
                        bw.newLine();
                        bw.close();
                        _logger.debug("Added blog comment: " + commentFilename);

                        // Send a Comment Email
                        sendCommentEmail(commentTitle, requestedCategory, permalink, commentAuthor, commentEmail,
                                commentLink, commentText, blog);
                    } catch (IOException e) {
                        _logger.error(e);
                        httpServletResponse.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, e.getMessage());
                    }
                } else {
                    _logger.error("Duplicate comment submission detected, ignoring subsequent submission");
                    httpServletResponse.sendError(HttpServletResponse.SC_FORBIDDEN, "Duplicate comment submission detected, ignoring subsequent submission");
                }

                httpServletResponse.setStatus(HttpServletResponse.SC_OK);
            } else {
                httpServletResponse.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, "No comment text available");
            }
        } else {
            httpServletResponse.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR, "Blog comments not enabled or No content in request");
        }
    }

    /**
     * Sends the comment as an Email to the Blog Author
     *
     * @param title       Entry title that this comment is for
     * @param category    Category for the entry
     * @param permalink   Permalink to the origional entry
     * @param author      Name of person commenting
     * @param authorEmail Email address of the person commenting
     * @param authorURL   Homepage URL for the person commenting
     * @param userComment The comment
     * @param blog        Users blog
     */
    private void sendCommentEmail(String title, String category, String permalink, String author,
                                  String authorEmail, String authorURL, String userComment, Blog blog) {

        String url = blog.getBlogURL() + BlojsomUtils.removeInitialSlash(category);
        String commentMessage = CommentUtils.constructCommentEmail(permalink, author, authorEmail, authorURL, userComment,
                url);
        try {
            EmailMessage emailMessage = new EmailMessage("[blojsom] Comment on: " + title, commentMessage);
            InternetAddress defaultRecipient = new InternetAddress(blog.getBlogOwnerEmail(), blog.getBlogOwner());
            EmailUtils.sendMailMessage(_mailsession, emailMessage, defaultRecipient);
        } catch (UnsupportedEncodingException e) {
            _logger.error(e);
        }
    }

    /**
     * Called when removing the servlet from the servlet container
     */
    public void destroy() {
    }
}

