--- lib/rexml/entity.rb	2008/09/13 01:55:56	19319
+++ lib/rexml/entity.rb	2008/09/13 02:07:42	19320
@@ -73,6 +73,7 @@
 		# all entities -- both %ent; and &ent; entities.  This differs from
 		# +value()+ in that +value+ only replaces %ent; entities.
 		def unnormalized
+      document.record_entity_expansion
 			v = value()
 			return nil if v.nil?
 			@unnormalized = Text::unnormalize(v, parent)
