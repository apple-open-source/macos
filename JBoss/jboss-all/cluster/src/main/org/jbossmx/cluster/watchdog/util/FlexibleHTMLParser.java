/**
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license Version 2.1, February 1999.
 * See terms of license at gnu.org.
 */

package org.jbossmx.cluster.watchdog.util;

import com.sun.jdmk.comm.HtmlParser;

import javax.management.MBeanRegistration;
import javax.management.ObjectName;
import javax.management.ObjectInstance;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;

import java.util.Set;

/**
 * @author Stacy Curl
 */
public class FlexibleHTMLParser
    implements HtmlParser, FlexibleHTMLParserMBean, MBeanRegistration
{
    /**
     */
    public FlexibleHTMLParser()
    {
        this("");
    }

    public FlexibleHTMLParser(String name)
    {
        m_name = name;
        m_head = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n"
             + "<HTML>\r\n" + "<HEAD>\r\n" + "<TITLE>" + m_name + " Agent View</TITLE>\r\n"
             + "</HEAD>\r\n";
    }

    /**
     * @param    in
     *
     * @return
     */
    public String parseRequest(String in)
    {
        String output = null;

        if((in != null) && (in.equals("/") || in.startsWith("/Filter")))
        {
            try
            {
                output = buildPage(in);
            }
            catch(Exception e)
            {
                e.printStackTrace();
            }
        }

        return output;
    }

    /**
     * @param    in
     *
     * @return
     */
    public String parsePage(String in)
    {
        return "croak(" + in + ")";
    }

    /**
     * @param    uri
     *
     * @return
     */
    public String buildPage(String uri)
    {
        StringBuffer page = new StringBuffer();

//        if(isTraceOn())
//            trace("buildPage", "Handle request = " + uri);
        page.append(getHead());
        page.append("<BODY><TABLE WIDTH=100%><TR>");
        page.append("<TD VALIGN=middle><H2>Agent View</H2></TD>");
        page.append("<TD ALIGN=right VALIGN=top>" + m_name + "</TD></TR></TABLE>");
        page.append("<FORM ACTION=/Filter METHOD=GET>");

        ObjectName objectname = null;
        if(uri.startsWith("/Filter"))
        {
            String s1 = /*decodeUrl(*/ uri.substring("/Filter".length() + 1 + 5);    //);
            try
            {
                objectname = new ObjectName(s1);
            }
            catch(MalformedObjectNameException malformedobjectnameexception)
            {
//                if(isDebugOn())
//                    debug("buildPage", "Exception = " + malformedobjectnameexception);
                return "By eck, an error occured: ";
//                buildError("Invalid Filter [" + s1 + "]", "474 Malformed ObjectName");
//                return;
            }

            page.append("Filter by object name: <INPUT type=text name=fstr value=" + s1 + ">");
        }
        else
        {
            page.append("Filter by object name: <INPUT type=text name=fstr value=*:*>");
        }

        page.append("</FORM><BR>");

        page.append("<TABLE WIDTH=100%><TR><TD>This agent is registered on the domain ");
        page.append("<STRONG><EM>" + m_server.getDefaultDomain() + "</EM></STRONG>.");

        Set objectInstances = m_server.queryMBeans(objectname, null);

        if(objectInstances == null)
        {
            page.append("<BR>This page contains no MBeans.</TD></TR></TABLE><HR>");
        }
        else
        {
            page.append("<BR>This page contains <STRONG>" + objectInstances.size() + "</STRONG>"
                        + " MBean(s).</TD>");
            page.append("<TD ALIGN=\"right\">");
            page.append("<FORM ACTION=\"/Admin/Main/\" METHOD=GET>");
            page.append("<INPUT TYPE=submit VALUE=\"Admin\">");
            page.append("</FORM></TD></TR></TABLE>");
            page.append("<HR><H4>List of registered MBeans by domain:</H4>");

            String s2 = null;
            boolean flag = false;

//            final int i = objectInstances.size();
            final int numberOfObjectInstances = objectInstances.size();

//            Object aobj[] = objectInstances.toArray();
            Object objectInstancesArray[] = objectInstances.toArray();
            bubbleSort(objectInstancesArray);

            String previousDomain = ((ObjectInstance) objectInstancesArray[0]).getObjectName()
                .getDomain();

            page.append("<UL type=circle><LI><STRONG>" + previousDomain
                        + "</STRONG></LI><UL type=disc>");

            for(int loop = 0; loop < objectInstancesArray.length; ++loop)
            {
                final String currentDomain = ((ObjectInstance) objectInstancesArray[loop])
                    .getObjectName().getDomain();

                if((loop != 0) && !currentDomain.equals(previousDomain))
                {
                    page.append("</UL><P><LI><STRONG>" + currentDomain
                                + "</STRONG></LI><UL type=disc>");

                    previousDomain = currentDomain;
                }

                final String objectNameLink = getObjectNameLink(
                    (ObjectInstance) objectInstancesArray[loop]);
                page.append(objectNameLink);
            }

            page.append("</UL></UL>");

//            while(!flag)
//            {
//                int l;
//                for(l = 0; l < numberOfObjectInstances; l++)
//                {
//                    String s5 = as[l];
//                    int j = s5.indexOf(58);
//                    if(j < 0)
//                        j = 0;
//                    String s3 = s5.substring(0, j);
//                    String s4 = s5.substring(j + 1);
//                    s4 = translateNameToHtmlFormat(s4);
//                    if(!s3.equals(s2) || s2 == null)
//                    {
//                        if(s2 == null)
//                            page.append("<UL type=circle>");
//                        else
//                            page.append("</UL><P>");
//                        s2 = new String(s3);
//                        page.append("<LI><STRONG>" + s2 + "</STRONG>");
//                        page.append("<UL type=disc>");
//                    }
//                    page.append("<LI><A HREF=\"/ViewObjectRes" + toUrlName(s5) + "\">" + s4 + "</A>");
//                }
//
//                if(l == numberOfObjectInstances)
//                {
//                    flag = true;
//                    page.append("</UL></UL>");
//                }
//                else
//                {
//                    s2 = null;
//                }
//            }
        }

        page.append("</BODY>\r\n</HTML>\r\n");

        String result = page.toString();
        System.out.println("Frog.result = " + result);

        return result;
    }

    /**
     * @param    objectInstance
     *
     * @return
     */
    private String getObjectNameLink(final ObjectInstance objectInstance)
    {
        final ObjectName objectName = objectInstance.getObjectName();

        return "<LI><A HREF=\"/ViewObjectRes" + objectName.toString() + "\">"
               + getFunkyDisplay(objectInstance) + "</A></LI>";
    }

    /**
     * @param    objectInstance
     *
     * @return
     */
    private String getFunkyDisplay(final ObjectInstance objectInstance)
    {
        String result;

        final ObjectName objectName = objectInstance.getObjectName();

        String objectNameString = objectName.toString();
        String noDomain = objectNameString.substring(objectName.getDomain().toString().length()
                              + 1);

        result = noDomain;

        try
        {
            Class objectInstanceClass = Class.forName(objectInstance.getClassName());
            if(org.jbossmx.cluster.watchdog.util.SkinableMBean.class
                .isAssignableFrom(objectInstanceClass))
            {
                result = (String) m_server.invoke(objectName, "retrieveOneLiner",
                                                  new Object[]{ noDomain },
                                                  new String[]{ "java.lang.String" });
            }
        }
        catch(Exception e) {}

        return result;
    }

    /**
     * Ya gotta love it, it's bubblesort!
     */
    private void bubbleSort(Object[] objectInstances)
    {
        final int numberOfObjectInstances = objectInstances.length;

        for(int outerLoop = numberOfObjectInstances - 1; outerLoop > 0; --outerLoop)
        {
            for(int innerLoop = 0; innerLoop < outerLoop; ++innerLoop)
            {
                final ObjectInstance first = (ObjectInstance) objectInstances[innerLoop];
                final ObjectInstance second = (ObjectInstance) objectInstances[innerLoop + 1];
                final String firstName = first.getObjectName().toString();
                final String secondName = second.getObjectName().toString();

                if(firstName.compareTo(secondName) > 0)
                {
                    objectInstances[innerLoop] = second;
                    objectInstances[innerLoop + 1] = first;
                }
            }
        }
    }

    /**
     * @param    server
     * @param    objectName
     *
     * @return
     */
    public ObjectName preRegister(MBeanServer server, ObjectName objectName)
    {
        m_server = server;

        return objectName;
    }

    /**
     *
     * @return
     */
    private String getHead()
    {
        return m_head;
    }

    /**
     */
    public void preDeregister() {}

    /**
     */
    public void postDeregister() {}

    /**
     * @param    bool
     */
    public void postRegister(Boolean bool) {}

    /** */
    private MBeanServer m_server;

    /** */
    private String m_head;

    /** */
    private String m_name;
}
