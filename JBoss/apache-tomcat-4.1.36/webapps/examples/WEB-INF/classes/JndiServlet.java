/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 
/* $Id: JndiServlet.java 466595 2006-10-21 22:24:41Z markt $
 *
 */

import java.io.IOException;
import java.io.PrintWriter;
import javax.servlet.*;
import javax.servlet.http.*;
import javax.naming.NamingException;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingEnumeration;

/**
 * Demonstration of the web application environment support.
 *
 * @author Remy Maucherat
 */

public class JndiServlet 
    extends HttpServlet {


    public void doGet(HttpServletRequest request, HttpServletResponse response)
        throws ServletException, IOException {
        
        PrintWriter out = response.getWriter();
        response.setContentType("text/plain");
        
        Context ctx = null;
        
        try {
            ctx = new InitialContext();
        } catch (NamingException e) {
            out.println("Couldn't build an initial context : " + e);
            return;
        }
        
        try {
            Object value = ctx.lookup("java:/comp/env/maxExemptions");
            out.println("Simple lookup test : ");
            out.println("Max exemptions value : " + value);
        } catch (NamingException e) {
            out.println("JNDI lookup failed : " + e);
        }
        
        try {
            Object value = ctx.lookup("java:/comp/env/linkToGlobalResource");
            out.println("Resource link test : ");
            out.println("Link value : " + value);
        } catch (NamingException e) {
            out.println("JNDI lookup failed : " + e);
        }
        
        try {
            Context envCtx = (Context) ctx.lookup("java:/comp/env/");
            out.println("list() on /comp/env Context : ");
            NamingEnumeration enumeration = ctx.list("java:/comp/env/");
            while (enumeration.hasMoreElements()) {
                out.print("Binding : ");
                out.println(enumeration.nextElement().toString());
            }
            out.println("listBindings() on /comp/env Context : ");
            enumeration = ctx.listBindings("java:/comp/env/");
            while (enumeration.hasMoreElements()) {
                out.print("Binding : ");
                out.println(enumeration.nextElement().toString());
            }
        } catch (NamingException e) {
            out.println("JNDI lookup failed : " + e);
        }
        
    }
    
    
}

