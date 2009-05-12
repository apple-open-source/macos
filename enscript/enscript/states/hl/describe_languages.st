/*
 * Describe all known highlight languages.
 */

state describe_me
{
  / \*$/ {
  }

  / \*\\\/.*/ {
    print ("\n");
    /* All done. */
    return;
  }

  / \* ?(.*)/ {
    print ($1);
  }
}

state describe_languages
{
  /^\/\*\*.*$/ {
    call (describe_me);
  }
  /[^\\\/]+/ {
    /* NOP */
  }
  /./ {
    /* NOP */
  }
}

/*
 * Create a HTML report of all supported highlighting rules.
 */

sub html_annotate_mailtos (str)
{
  return regsuball (str, /[-_a-zA-Z0-9\\.]+@[-_a-zA-Z0-9\\.]+/,
		    "<a href=\"mailto:$0\">$0</a>");
}

sub html_quote (str)
{
  str = regsuball (str, /\&/, "&amp;");
  str = regsuball (str, /</, "&lt;");
  str = regsuball (str, />/, "&gt;");
  str = regsuball (str, /\"/, "&quot;");
  return str;
}

sub describe_me_html_print_pending_name ()
{
  if (!language_name_pending)
    return;

  print ("<p>\n<dl compact>\n<dt><b>Name:</b><dd>",
	 html_quote (language_name), "\n");

  language_name_pending = 0;
}

state describe_me_html
{
  / \*$/ {
  }

  / \*\\\/.*/ {
    /* Terminate this state. */
    describe_me_html_print_pending_name ();
    print ("</dl>\n");
    return;
  }

  / \* ?(.*)/ {
    row = $1;
    if (regmatch (row, /Name:(.*)/))
      {
	language_name = $1;
	language_name_pending = 1;
      }
    else if (regmatch (row, /Description:(.*)/))
      {
	/* This starts the new language. */
	title = $1;
	title = regsub (title, /^[ \t]+/, "");
	title = regsub (title, /[ \t\\.]+$/, "");
	print ("<p><li><b>", html_quote (title), "</b><p>\n");
      }
    else if (regmatch (row, /([a-zA-Z]+:)(.*)/))
      {
	describe_me_html_print_pending_name ();
	print ("<dt><b>", html_quote ($1), "</b><dd>",
	       html_annotate_mailtos (html_quote ($2)));
      }
    else
      print (html_annotate_mailtos (html_quote (row)));
  }
}

current_input_file = 0;

state describe_languages_html
{
  BEGIN {
    if (current_input_file == 0)
      {
	title = "Enscript Highlighting Languages And File Formats";
	print ("<!DOCTYPE html PUBLIC \"-//IETF//DTD HTML 2.0//EN\">\n",
	       "<html>\n<head>\n<title>", title, "</title>\n",
	       "<LINK REV=\"made\" HREF=\"mailto:mtr@iki.fi\">\n",
	       "</head>\n",
	       "<body BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\" LINK=\"#1F00FF\"",
	       "ALINK=\"#FF0000\" VLINK=\"#9900DD\">\n",
	       "<h1>", title, "</h1>\n<hr>\n<ul>\n");
      }
    current_input_file++;
  }

  END {
    if (current_input_file == length (argv))
      print ("\n</ul>\n<hr><address>By ", version,
	     "</address>\n</body>\n</html>\n");
  }

  /^\/\*\*.*$/ {
    call (describe_me_html);
  }

  /[^\\\/]+/ {
    /* NOP */
  }

  /./ {
    /* NOP */
  }
}


/*
Local variables:
mode: c
End:
*/
