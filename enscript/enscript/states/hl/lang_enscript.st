
state lang_enscript
{
  BEGIN {
    /* RGB -> PostScript color mapper function. */
    sub map_color (r, g, b)
      {
	return sprintf ("%f %f %f", r div 255.0, g div 255.0, b div 255.0);
      }

    /* No language special characters. */
    LANGUAGE_SPECIALS = 0;

    sub language_print (str)
      {
	print (str);
      }

    sub symbol (sym)
      {
	print ("\0font{Symbol", ptsize, ":ps}", sym, "\0font{default}");
      }

    sub language_symbol (sym)
      {
	local result = true;

	if (strcmp (sym, "rightarrow") == 0)
	  symbol ("\0256");
	else if (strcmp (sym, "le") == 0)
	  symbol ("\0243");
	else if (strcmp (sym, "ge") == 0)
	  symbol ("\0263");
	else if (strcmp (sym, "ne") == 0)
	  symbol ("\0271");
	else if (strcmp (sym, "equiv") == 0)
	  symbol ("\0272");
	else if (strcmp (sym, "land") == 0)
	  symbol ("\0331");
	else if (strcmp (sym, "lor") == 0)
	  symbol ("\0332");
	else if (strcmp (sym, "lnot") == 0)
	  symbol ("\0330");
	else
	  result = false;

	return result;
      }

    sub header ()
      {
	/* Nothing here. */
      }

    sub trailer ()
      {
	/* Nothing here. */
      }

    sub font (name)
      {
	print ("\0font{", name, "@", ptsize, "}");
      }

    sub color (name)
      {
	print ("\0color{", name, "}");
      }

    sub bgcolor (name)
      {
	print ("\0bgcolor{", name, "}");
      }

    sub face_on (face)
      {
	font (face[fontname]);
	if (face[fg_color] || face[bg_color])
	  {
	    if (face[fg_color])
	      color (face[fg_color]);
	    if (face[bg_color])
	      bgcolor (face[bg_color]);
	  }
      }

    sub face_off (face)
      {
	print ("\0font{default}");
	if (face[fg_color] || face[bg_color])
	  {
	    if (face[fg_color])
	      print ("\0color{default}");
	    if (face[bg_color])
	      {
		print ("\0bgcolor{default}");
	      }
	  }
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
