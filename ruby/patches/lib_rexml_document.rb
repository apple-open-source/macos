--- lib/rexml/document.rb	2008/09/13 01:55:56	19319
+++ lib/rexml/document.rb	2008/09/13 02:07:42	19320
@@ -32,6 +32,7 @@
 	  # @param context if supplied, contains the context of the document;
 	  # this should be a Hash.
 		def initialize( source = nil, context = {} )
+      @entity_expansion_count = 0
 			super()
 			@context = context
 			return if source.nil?
@@ -200,6 +201,27 @@
 			Parsers::StreamParser.new( source, listener ).parse
 		end
 
+    @@entity_expansion_limit = 10_000
+
+    # Set the entity expansion limit. By default the limit is set to 10000.
+    def Document::entity_expansion_limit=( val )
+      @@entity_expansion_limit = val
+    end
+
+    # Get the entity expansion limit. By default the limit is set to 10000.
+    def Document::entity_expansion_limit
+      return @@entity_expansion_limit
+    end
+
+    attr_reader :entity_expansion_count
+    
+    def record_entity_expansion
+      @entity_expansion_count += 1
+      if @entity_expansion_count > @@entity_expansion_limit
+        raise "number of entity expansions exceeded, processing aborted."
+      end
+    end
+
 		private
 		def build( source )
       Parsers::TreeParser.new( source, self ).parse
