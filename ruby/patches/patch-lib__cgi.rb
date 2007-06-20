--- lib/cgi.rb.old	2007-05-07 14:34:39.000000000 +0200
+++ lib/cgi.rb	2007-05-07 14:39:56.000000000 +0200
@@ -966,6 +966,7 @@
     def read_multipart(boundary, content_length)
       params = Hash.new([])
       boundary = "--" + boundary
+      quoted_boundary = Regexp.quote(boundary, "n")
       buf = ""
       bufsize = 10 * 1024
 
@@ -996,7 +997,7 @@
         end
         body.binmode if defined? body.binmode
 
-        until head and /#{boundary}(?:#{EOL}|--)/n.match(buf)
+        until head and /#{quoted_boundary}(?:#{EOL}|--)/n.match(buf)
 
           if (not head) and /#{EOL}#{EOL}/n.match(buf)
             buf = buf.sub(/\A((?:.|\n)*?#{EOL})#{EOL}/n) do
@@ -1016,14 +1017,14 @@
               else
                 stdinput.read(content_length)
               end
-          if c.nil?
+          if c.nil? || c.empty?
             raise EOFError, "bad content body"
           end
           buf.concat(c)
           content_length -= c.size
         end
 
-        buf = buf.sub(/\A((?:.|\n)*?)(?:[\r\n]{1,2})?#{boundary}([\r\n]{1,2}|--)/n) do
+        buf = buf.sub(/\A((?:.|\n)*?)(?:[\r\n]{1,2})?#{quoted_boundary}([\r\n]{1,2}|--)/n) do
           body.print $1
           if "--" == $2
             content_length = -1
@@ -1059,7 +1060,7 @@
           params[name] = [body]
         end
         break if buf.size == 0
-        break if content_length === -1
+        break if content_length == -1
       end
 
       params
