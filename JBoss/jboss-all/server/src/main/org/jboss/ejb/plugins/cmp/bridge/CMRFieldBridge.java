package org.jboss.ejb.plugins.cmp.bridge;

public interface CMRFieldBridge extends FieldBridge {
   public boolean isSingleValued();
   public boolean isCollectionValued();
   public EntityBridge getRelatedEntity();
}
