/*
 * Definitions for the ANSI color tty outputs.
 * Author: Bill Petheram <petheram@acm.org>
 *	   Markku Rossi <mtr@iki.fi>
 */

state lang_ansi
{
  BEGIN {
    /* The current face as an ANSI str. */
    ansi_face_str = 0;

    ansi_face_stack = list();
    ansi_face_stack_pos = 0;

    ansi_color_table = list();
    ansi_color_table_size = 0;

    sub ansi_define_color(ansi, r, g, b)
      {
	ansi_color_table[ansi_color_table_size] = list(ansi, r, g, b);
	ansi_color_table_size = ansi_color_table_size + 1;
      }

    ansi_define_color(30, 0,   0,   0); 	/* black */
    ansi_define_color(31, 205, 0,   0); 	/* red3 */
    ansi_define_color(32, 0,   205, 0);  	/* green3 */
    ansi_define_color(33, 205, 205, 0);		/* yellow3 */
    ansi_define_color(34, 0,   0,   205);	/* blue3 */
    ansi_define_color(35, 205, 0,   205);	/* magenta3 */
    ansi_define_color(36, 0,   205, 205);	/* cyan3 */
    ansi_define_color(37, 229, 229, 229);	/* gray90 */

    ansi_define_color(90, 77,  77,  77); 	/* gray30 */
    ansi_define_color(91, 255, 0,   0); 	/* red */
    ansi_define_color(92, 0,   255, 0);  	/* green */
    ansi_define_color(93, 255, 255, 0);		/* yellow */
    ansi_define_color(94, 0,   0,   255);	/* blue */
    ansi_define_color(95, 255, 0,   255);	/* magenta */
    ansi_define_color(96, 0,   255, 255);	/* cyan */
    ansi_define_color(97, 255, 255, 255);	/* white */

    sub ansi_count_delta(a, b)
      {
	local delta = a - b;

	if (delta < 0)
	  delta = 0 - delta;

	return delta * delta;
      }

    sub ansi_count_color_delta(ansi_color, r, g, b)
      {
	local delta = 0;

	delta += ansi_count_delta(ansi_color[1], r);
	delta += ansi_count_delta(ansi_color[2], g);
	delta += ansi_count_delta(ansi_color[3], b);

	return delta;
      }

    sub map_color(r, g, b)
      {
	local i, min = 65536, min_color = 0, delta;

	for (i = 0; i < ansi_color_table_size; i++)
	  {
	    delta = ansi_count_color_delta(ansi_color_table[i], r, g, b);
	    if (delta < min)
	      {
		min_color = ansi_color_table[i][0];
		min = delta;
	      }
	  }
	if (min > 16384)
	  /* Don't accept too bad matches. */
	  min_color = 0;

	return min_color;
      }

    LANGUAGE_SPECIALS = /\n/;

    sub language_print(str)
      {
	if (ansi_face_str)
	  str = regsuball(str, /\n/, concat("\033[0m\n", ansi_face_str));

	print(str);
      }

    sub header()
      {
      }

    sub trailer()
      {
	print("\033[0m");
      }

    sub ansi_set_face(face)
      {
	local b = "", i = "", fg ="", bg = "";

	if (face[boldp])
	  b = ";1";
	if (face[italicp])
	  i = ";4";
	if (face[fg_color])
	  fg = concat(";", string(face[fg_color]));
	if (face[bg_color])
	  bg = concat(";", string(face[bg_color] + 10));

	ansi_face_str = concat("\033[0", b, i, fg, bg, "m");

	print(ansi_face_str);
      }

    sub face_on(face)
      {
	ansi_face_stack[ansi_face_stack_pos++] = face;
	ansi_set_face(face);
      }

    sub face_off(face)
      {
	if (ansi_face_stack_pos > 0)
	  /* Just to make sure that we don't fail on some broken
             highlighting rules.  */
	  ansi_face_stack_pos--;

	if (ansi_face_stack_pos == 0)
	  {
	    print("\033[0m");
	    ansi_face_str = 0;
	  }
	else
	  ansi_set_face(ansi_face_stack[ansi_face_stack_pos - 1]);
      }

    return;
  }
}


/*
Local variables:
mode: c
End:
*/
