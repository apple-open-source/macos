/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
 
package org.jboss.ejb.plugins.cmp.ejbql;

import org.jboss.ejb.plugins.cmp.bridge.EntityBridge;

/**
 * This abstract syntax node represents an abstract schema name.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.1 $
 */                            
public class ASTAbstractSchema extends SimpleNode {
    public String abstractSchemaName;
    public EntityBridge entity;

    public ASTAbstractSchema(int id) {
         super(id);
    }

    public ASTAbstractSchema(EJBQLParser p, int id) {
         super(p, id);
    }


    /** Accept the visitor. **/
    public Object jjtAccept(JBossQLParserVisitor visitor, Object data) {
         return visitor.visit(this, data);
    }
}
