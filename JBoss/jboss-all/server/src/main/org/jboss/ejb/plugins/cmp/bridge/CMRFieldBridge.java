package org.jboss.ejb.plugins.cmp.bridge;

public interface CMRFieldBridge extends FieldBridge {
   public boolean isSingleValued();
   public EntityBridge getRelatedEntity();
}
