--- list.c	2014-11-17 19:09:21.000000000 -0800
+++ list.c	2014-11-17 19:08:51.000000000 -0800
@@ -327,7 +327,7 @@ int list_files(__G)    /* return PK-type
             if (methnum == DEFLATED || methnum == ENHDEFLATED) {
                 methbuf[5] = dtype[(G.crec.general_purpose_bit_flag>>1) & 3];
             } else if (methnum >= NUM_METHODS) {
-                sprintf(&methbuf[4], "%03u", G.crec.compression_method);
+                snprintf(&methbuf[4], sizeof(methbuf)-4, "%03u", G.crec.compression_method);
             }
 
 #if 0       /* GRR/Euro:  add this? */
