/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security;

import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.CodeSource;
import java.security.KeyStore;
import java.security.GeneralSecurityException;
import java.security.Permission;
import java.security.Principal;
import java.security.UnresolvedPermission;
import java.security.cert.Certificate;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.StringTokenizer;
import javax.security.auth.login.AppConfigurationEntry;
import javax.security.auth.login.AppConfigurationEntry.LoginModuleControlFlag;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.InputSource;
import org.xml.sax.EntityResolver;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.DocumentBuilder;


/** A class that parses a XML document that conforms to the security-policy.dtd
DTD that implements IAppPolicyStore for use with the SecurityPolicy class.

@author Scott.Stark@jboss.org
@version $Revision: 1.2.4.1 $
*/
public class SecurityPolicyParser implements IAppPolicyStore
{
    private static String DEFAULT_APP_POLICY_NAME = "other";
    private URL policyURL;
    private HashMap policyMap = new HashMap();

	/** Creates new SecurityPolicyParser */
    public SecurityPolicyParser(URL policyURL)
    {
        this.policyURL = policyURL;
    }


    public AppPolicy getAppPolicy(String appName)
    {
        AppPolicy appPolicy = (AppPolicy) policyMap.get(appName);
        if( appPolicy == null )
            appPolicy = AppPolicy.getDefaultAppPolicy();
        return appPolicy;
    }

    /** Load/reload the security policy
     */
    public void refresh()
    {
        Document doc = null;
        try
        {
            doc = loadURL();
        }
        catch(Exception e)
        {
            e.printStackTrace();
            return;
        }

        Element root = doc.getDocumentElement();
        NodeList apps = root.getElementsByTagName("application-policy");
        for(int n = 0; n < apps.getLength(); n ++)
        {
            Element app = (Element) apps.item(n);
            String name = app.getAttribute("name");
            AppPolicy appPolicy = new AppPolicy(name);
            try
            {
                parse(app, appPolicy);
                if( name.equals(DEFAULT_APP_POLICY_NAME) )
                    AppPolicy.setDefaultAppPolicy(appPolicy);
                else
                    policyMap.put(name, appPolicy);
            }
            catch(Exception e)
            {
                e.printStackTrace();
            }
        }
    }

    private Document loadURL() throws Exception
    {
        InputStream is = policyURL.openStream();
        DocumentBuilderFactory docBuilderFactory = DocumentBuilderFactory.newInstance();
        DocumentBuilder docBuilder = docBuilderFactory.newDocumentBuilder();
        EntityResolver resolver = new LocalResolver();
        docBuilder.setEntityResolver(resolver);
        Document doc = docBuilder.parse(is);
        return doc;
    }
    private void parse(Element policy, AppPolicy appPolicy) throws Exception
    {
        parseKeyStore(policy, appPolicy);
        parseAuthentication(policy, appPolicy);
        parseAuthorization(policy, appPolicy);
    }

    /** Parse the application-policy/keystore element
    @param policy, the application-policy element
    */
    private void parseKeyStore(Element policy, AppPolicy appPolicy) throws Exception
    {
        // Load the keystore
        NodeList keystore = policy.getElementsByTagName("keystore");
        String keystoreHref = ".keystore";
        String keystoreType = "JKS";
        if( keystore.getLength() > 0 )
        {   // Load the cert KeyStore. This needs more work to be complete...
            Element e = (Element) keystore.item(0);
            keystoreHref = e.getAttribute("href");
            keystoreType = e.getAttribute("type");
            InputStream keystoreStream = null;
            try
            {
                URL keystoreURL = new URL(keystoreHref);
                keystoreStream = keystoreURL.openStream();
            }
            catch(MalformedURLException ex)
            {   // Assume this is just a resource name and look for in
                ClassLoader loader = Thread.currentThread().getContextClassLoader();
                keystoreStream = loader.getResourceAsStream(keystoreHref);
            }
            KeyStore keyStore = KeyStore.getInstance(keystoreType);
            // Where to get the password from...
            char[] password = {};
            keyStore.load(keystoreStream, password);
            appPolicy.setKeyStore(keyStore);
        }
    }

    /** Parse the application-policy/authentication element
    @param policy, the application-policy element
    */
    private void parseAuthentication(Element policy, AppPolicy appPolicy) throws Exception
    {
        // Parse the permissions
        NodeList authentication = policy.getElementsByTagName("authentication");
        if( authentication.getLength() == 0 )
            return;
        Element auth = (Element) authentication.item(0);
        NodeList modules = auth.getElementsByTagName("login-module");
        ArrayList tmp = new ArrayList();
        for(int n = 0; n < modules.getLength(); n ++)
        {
            Element grant = (Element) modules.item(n);
            parseModule(grant, tmp);
        }
        AppConfigurationEntry[] entries = new AppConfigurationEntry[tmp.size()];
        tmp.toArray(entries);
        AuthenticationInfo info = new AuthenticationInfo();
        info.setAppConfigurationEntry(entries);
        appPolicy.setLoginInfo(info);
    }
    private void parseModule(Element module, ArrayList entries) throws Exception
    {
        LoginModuleControlFlag controlFlag = LoginModuleControlFlag.OPTIONAL;
        String className = module.getAttribute("code");
        String flag = module.getAttribute("flag");
        if( flag != null )
        {
            if( flag.equals(LoginModuleControlFlag.REQUIRED.toString()) )
                controlFlag = LoginModuleControlFlag.REQUIRED;
            else if( flag.equals(LoginModuleControlFlag.REQUISITE.toString()) )
                controlFlag = LoginModuleControlFlag.REQUISITE;
            else if( flag.equals(LoginModuleControlFlag.SUFFICIENT.toString()) )
                controlFlag = LoginModuleControlFlag.SUFFICIENT;
            else if( flag.equals(LoginModuleControlFlag.OPTIONAL.toString()) )
                controlFlag = LoginModuleControlFlag.OPTIONAL;
        }
        NodeList opts = module.getElementsByTagName("module-option");
        HashMap options = new HashMap();
        for(int n = 0; n < opts.getLength(); n ++)
        {
            Element opt = (Element) opts.item(n);
            String name = opt.getAttribute("name");
            String value = getContent(opt, "");
            options.put(name, value);
        }
        AppConfigurationEntry entry = new AppConfigurationEntry(className, controlFlag, options);
        entries.add(entry);
    }

    /** Parse the application-policy/authorization element
    @param policy, the application-policy element
    */
    private void parseAuthorization(Element policy, AppPolicy appPolicy) throws Exception
    {
        // Parse the permissions
        NodeList authorization = policy.getElementsByTagName("authorization");
        if( authorization.getLength() == 0 )
            return;
        Element auth = (Element) authorization.item(0);
        NodeList grants = auth.getElementsByTagName("grant");
        for(int n = 0; n < grants.getLength(); n ++)
        {
            Element grant = (Element) grants.item(n);
            parseGrant(grant, appPolicy);
        }
    }
    private void parseGrant(Element grant, AppPolicy appPolicy) throws Exception
    {
        // Look for the codebase
        URL codebase = null;
        if( grant.getAttribute("codebase") != null )
        {
            String attr = grant.getAttribute("codebase");
            if( attr.length() > 0 )
                codebase = new URL(attr);
        }
        // Look for the code signers
        String[] signerAliases = {};
        Certificate[] signedBy = null;
        if( grant.getAttribute("signedBy") != null )
        {
            String signers = grant.getAttribute("signedBy");
            if( signers.length() > 0 )
                signedBy = getCertificates(signers, appPolicy.getKeyStore());
        }
        CodeSource cs = new CodeSource(codebase, signedBy);

        // Look for the principals
        ArrayList principals = null;
        NodeList tmp = grant.getElementsByTagName("principal");
        ClassLoader loader = Thread.currentThread().getContextClassLoader();
        for(int n = 0; n < tmp.getLength(); n ++)
        {
            Element principal = (Element) tmp.item(n);
            String code = principal.getAttribute("code");
            String name = principal.getAttribute("name");
            try
            {
                Class cls = loader.loadClass(code);
                // Assume there exists a constructor(String)
                Class[] parameterTypes = {String.class};
                Constructor ctor = cls.getConstructor(parameterTypes);
                Object[] args = {name};
                Principal p = (Principal) ctor.newInstance(args);
                if( principals == null )
                    principals = new ArrayList();
                principals.add(p);
            }
            catch(Exception e)
            {
                throw new GeneralSecurityException(e.getClass().getName()+','+e.getMessage());
            }
        }

        // Get the permissions
        ArrayList permissions = null;
        tmp = grant.getElementsByTagName("permission");
        for(int n = 0; n < tmp.getLength(); n ++)
        {
            Element perm = (Element) tmp.item(n);
            String code = perm.getAttribute("code");
            String name = perm.getAttribute("name");
            String actions = perm.getAttribute("actions");
            String signers = perm.getAttribute("signedBy"); // Currently ignored...
            name = expandString(name);
            try
            {
                Class cls = null;
                // Assume there exists a ctor(String) or a ctor(String, String)
                Constructor ctor = null;
                Permission p = null;
                try
                {
                    cls = loader.loadClass(code);
                    Class[] parameterTypes2 = {String.class, String.class};
                    Object[] args2 = {name, actions};
                    ctor = cls.getConstructor(parameterTypes2);
                    p = (Permission) ctor.newInstance(args2);
                }
                catch(ClassNotFoundException e)
                {   // Use an UnresolvedPermission
                    Certificate[] certs = null;
                    if( signers != null )
                        certs = getCertificates(signers, appPolicy.getKeyStore());
                    p = new UnresolvedPermission(code, name, actions, certs);
                }
                catch(Exception e)
                {   // Try ctor(String)
                    Class[] parameterTypes = {String.class};
                    Object[] args = {name};
                    ctor = cls.getConstructor(parameterTypes);
                    p = (Permission) ctor.newInstance(args);
                }
                if( permissions == null )
                    permissions = new ArrayList();
                if( p != null )
                    permissions.add(p);
            }
            catch(Exception e)
            {
                throw new GeneralSecurityException(e.getClass().getName()+','+e.getMessage());
            }
        }

        Principal[] roles = new Principal[0];
        AuthorizationInfo authInfo = appPolicy.getPermissionInfo();
        if( authInfo == null )
        {
            authInfo = new AuthorizationInfo();
            appPolicy.setPermissionInfo(authInfo);
        }

        if( principals == null )
        {
            authInfo.grant(cs, permissions);
        }
        else
        {
            roles = (Principal[]) principals.toArray(roles);
            authInfo.grant(cs, permissions, roles);
        }
    }

    private String expandString(String str)
    {
        int index = str.indexOf("${/}");
        if( index >= 0 )
        {
            int start = 0;
            StringBuffer sb = new StringBuffer();
            while( index >= 0 )
            {
                sb.append(str.substring(start, index));
                sb.append(java.io.File.separatorChar);
                start = index + 4;
                index = str.indexOf("${/}", start);
            }
            if( start <= str.length()-1 )
                sb.append(str.substring(start));
            str = sb.toString();
        }
        return str;
    }

    private Certificate[] getCertificates(String signedBy, KeyStore keyStore)
    {
        Certificate[] signedByCerts = null;
        StringTokenizer tokenizer = new StringTokenizer(signedBy, ",");
        ArrayList certs = new ArrayList();
        while( tokenizer.hasMoreTokens() )
        {
            String alias = tokenizer.nextToken();
            try
            {
                Certificate cert = keyStore.getCertificate(alias);
                certs.add(cert);
            }
            catch(GeneralSecurityException e)
            {
                e.printStackTrace();
            }
        }
        if( certs.size() > 0 )
        {
            signedByCerts = new Certificate[certs.size()];
            certs.toArray(signedByCerts);
        }
        return signedByCerts;
    }

	public static String getContent(Element element, String defaultContent)
    {
		NodeList children = element.getChildNodes();
        String content = defaultContent;
        if( children.getLength() > 0 )
        {
            content = "";
            for(int n = 0; n < children.getLength(); n ++)
            {
                if( children.item(n).getNodeType() == Node.TEXT_NODE || 
                    children.item(n).getNodeType() == Node.CDATA_SECTION_NODE )
                   content += children.item(n).getNodeValue();
                else
                   content += children.item(n).getFirstChild();   
            }
            return content;
        }
        return content;
	}

	/** Local entity resolver to handle the security-policy DTD public id.
	*/
	private static class LocalResolver implements EntityResolver
	{
		private static final String SECURITY_POLICY_PUBLIC_ID = "-//JBoss//DTD JAAS SecurityPolicy//EN";
		private static final String SECURITY_POLICY_DTD_NAME = "security-policy.dtd";

		public InputSource resolveEntity(String publicId, String systemId)
		{
			InputSource is = null;
			if( publicId.equals(SECURITY_POLICY_PUBLIC_ID) )
			{
				try
				{
					InputStream dtdStream = getClass().getResourceAsStream(SECURITY_POLICY_DTD_NAME);
					is = new InputSource(dtdStream);
				}
				catch(Exception ex )
				{
				}
			}
			return is;
		}
	}
}
