<?xml version='1.0' encoding='ISO-8859-1' standalone='yes'?>
<tagfile>
  <compound kind="file">
    <name>array.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>array_8h</filename>
    <includes id="sequence_8h" name="sequence.h" local="no">gimp-print/sequence.h</includes>
    <member kind="typedef">
      <type>stp_array</type>
      <name>stp_array_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create</name>
      <anchor>ga1</anchor>
      <arglist>(int x_size, int y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_destroy</name>
      <anchor>ga2</anchor>
      <arglist>(stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_copy</name>
      <anchor>ga3</anchor>
      <arglist>(stp_array_t *dest, const stp_array_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create_copy</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_set_size</name>
      <anchor>ga5</anchor>
      <arglist>(stp_array_t *array, int x_size, int y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_get_size</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_array_t *array, int *x_size, int *y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_set_data</name>
      <anchor>ga7</anchor>
      <arglist>(stp_array_t *array, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_get_data</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_array_t *array, size_t *size, const double **data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_array_set_point</name>
      <anchor>ga9</anchor>
      <arglist>(stp_array_t *array, int x, int y, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_array_get_point</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_array_t *array, int x, int y, double *data)</arglist>
    </member>
    <member kind="function">
      <type>const stp_sequence_t *</type>
      <name>stp_array_get_sequence</name>
      <anchor>ga11</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>bit-ops.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>bit-ops_8h</filename>
    <member kind="function">
      <type>void</type>
      <name>stp_fold</name>
      <anchor>a0</anchor>
      <arglist>(const unsigned char *line, int single_height, unsigned char *outbuf)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_split_2</name>
      <anchor>a1</anchor>
      <arglist>(int height, int bits, const unsigned char *in, unsigned char *outhi, unsigned char *outlo)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_split_4</name>
      <anchor>a2</anchor>
      <arglist>(int height, int bits, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unpack_2</name>
      <anchor>a3</anchor>
      <arglist>(int height, int bits, const unsigned char *in, unsigned char *outlo, unsigned char *outhi)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unpack_4</name>
      <anchor>a4</anchor>
      <arglist>(int height, int bits, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unpack_8</name>
      <anchor>a5</anchor>
      <arglist>(int height, int bits, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3, unsigned char *out4, unsigned char *out5, unsigned char *out6, unsigned char *out7)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>channel.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>channel_8h</filename>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_reset</name>
      <anchor>a0</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_reset_channel</name>
      <anchor>a1</anchor>
      <arglist>(stp_vars_t *v, int channel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_add</name>
      <anchor>a2</anchor>
      <arglist>(stp_vars_t *v, unsigned channel, unsigned subchannel, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_density_adjustment</name>
      <anchor>a3</anchor>
      <arglist>(stp_vars_t *v, int color, int subchannel, double adjustment)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_ink_limit</name>
      <anchor>a4</anchor>
      <arglist>(stp_vars_t *v, double limit)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_cutoff_adjustment</name>
      <anchor>a5</anchor>
      <arglist>(stp_vars_t *v, int color, int subchannel, double adjustment)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_black_channel</name>
      <anchor>a6</anchor>
      <arglist>(stp_vars_t *v, int channel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_initialize</name>
      <anchor>a7</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int input_channel_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_convert</name>
      <anchor>a8</anchor>
      <arglist>(const stp_vars_t *v, unsigned *zero_mask)</arglist>
    </member>
    <member kind="function">
      <type>unsigned short *</type>
      <name>stp_channel_get_input</name>
      <anchor>a9</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>unsigned short *</type>
      <name>stp_channel_get_output</name>
      <anchor>a10</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>color.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>color_8h</filename>
    <class kind="struct">stp_colorfuncs_t</class>
    <class kind="struct">stp_color</class>
    <member kind="typedef">
      <type>stp_color</type>
      <name>stp_color_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_init</name>
      <anchor>ga1</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, size_t steps)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_get_row</name>
      <anchor>ga2</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int row, unsigned *zero_mask)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_color_list_parameters</name>
      <anchor>ga3</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_color_describe_parameter</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_register</name>
      <anchor>ga5</anchor>
      <arglist>(const stp_color_t *color)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_unregister</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_color_t *color)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_count</name>
      <anchor>ga7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_name</name>
      <anchor>ga8</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_index</name>
      <anchor>ga9</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_colorfuncs</name>
      <anchor>ga10</anchor>
      <arglist>(stp_colorfuncs_t *colorfuncs)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_color_get_name</name>
      <anchor>ga11</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_color_get_long_name</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>curve-cache.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>curve-cache_8h</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stp_cached_curve_t</class>
    <member kind="define">
      <type>#define</type>
      <name>CURVE_CACHE_FAST_USHORT</name>
      <anchor>a0</anchor>
      <arglist>(cache)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CURVE_CACHE_FAST_DOUBLE</name>
      <anchor>a1</anchor>
      <arglist>(cache)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CURVE_CACHE_FAST_COUNT</name>
      <anchor>a2</anchor>
      <arglist>(cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_free_curve_cache</name>
      <anchor>a3</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_curve_data</name>
      <anchor>a4</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_cache_get_curve</name>
      <anchor>a5</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_curve_invalidate</name>
      <anchor>a6</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_set_curve</name>
      <anchor>a7</anchor>
      <arglist>(stp_cached_curve_t *cache, stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_set_curve_copy</name>
      <anchor>a8</anchor>
      <arglist>(stp_cached_curve_t *cache, const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>const size_t</type>
      <name>stp_curve_cache_get_count</name>
      <anchor>a9</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned short *</type>
      <name>stp_curve_cache_get_ushort_data</name>
      <anchor>a10</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>const double *</type>
      <name>stp_curve_cache_get_double_data</name>
      <anchor>a11</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_copy</name>
      <anchor>a12</anchor>
      <arglist>(stp_cached_curve_t *dest, const stp_cached_curve_t *src)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>curve.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>curve_8h</filename>
    <includes id="sequence_8h" name="sequence.h" local="no">gimp-print/sequence.h</includes>
    <class kind="struct">stp_curve_point_t</class>
    <member kind="typedef">
      <type>stp_curve</type>
      <name>stp_curve_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_type_t</name>
      <anchor>ga47</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_TYPE_LINEAR</name>
      <anchor>gga47a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_TYPE_SPLINE</name>
      <anchor>gga47a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_wrap_mode_t</name>
      <anchor>ga48</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_WRAP_NONE</name>
      <anchor>gga48a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_WRAP_AROUND</name>
      <anchor>gga48a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_compose_t</name>
      <anchor>ga49</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_COMPOSE_ADD</name>
      <anchor>gga49a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_COMPOSE_MULTIPLY</name>
      <anchor>gga49a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_COMPOSE_EXPONENTIATE</name>
      <anchor>gga49a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_bounds_t</name>
      <anchor>ga50</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_BOUNDS_RESCALE</name>
      <anchor>gga50a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_BOUNDS_CLIP</name>
      <anchor>gga50a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_BOUNDS_ERROR</name>
      <anchor>gga50a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create</name>
      <anchor>ga1</anchor>
      <arglist>(stp_curve_wrap_mode_t wrap)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_copy</name>
      <anchor>ga2</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_copy</name>
      <anchor>ga3</anchor>
      <arglist>(stp_curve_t *dest, const stp_curve_t *source)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_destroy</name>
      <anchor>ga4</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_bounds</name>
      <anchor>ga5</anchor>
      <arglist>(stp_curve_t *curve, double low, double high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_get_bounds</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_curve_t *curve, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_wrap_mode_t</type>
      <name>stp_curve_get_wrap</name>
      <anchor>ga7</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_is_piecewise</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_get_range</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_curve_t *curve, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_curve_count_points</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_interpolation_type</name>
      <anchor>ga11</anchor>
      <arglist>(stp_curve_t *curve, stp_curve_type_t itype)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_type_t</type>
      <name>stp_curve_get_interpolation_type</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_data</name>
      <anchor>ga13</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_data_points</name>
      <anchor>ga14</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const stp_curve_point_t *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_float_data</name>
      <anchor>ga15</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const float *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_long_data</name>
      <anchor>ga16</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_ulong_data</name>
      <anchor>ga17</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const unsigned long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_int_data</name>
      <anchor>ga18</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_uint_data</name>
      <anchor>ga19</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const unsigned int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_short_data</name>
      <anchor>ga20</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const short *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_ushort_data</name>
      <anchor>ga21</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const unsigned short *data)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_get_subrange</name>
      <anchor>ga22</anchor>
      <arglist>(const stp_curve_t *curve, size_t start, size_t count)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_subrange</name>
      <anchor>ga23</anchor>
      <arglist>(stp_curve_t *curve, const stp_curve_t *range, size_t start)</arglist>
    </member>
    <member kind="function">
      <type>const double *</type>
      <name>stp_curve_get_data</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const stp_curve_point_t *</type>
      <name>stp_curve_get_data_points</name>
      <anchor>ga25</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const float *</type>
      <name>stp_curve_get_float_data</name>
      <anchor>ga26</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const long *</type>
      <name>stp_curve_get_long_data</name>
      <anchor>ga27</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned long *</type>
      <name>stp_curve_get_ulong_data</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const int *</type>
      <name>stp_curve_get_int_data</name>
      <anchor>ga29</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned int *</type>
      <name>stp_curve_get_uint_data</name>
      <anchor>ga30</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const short *</type>
      <name>stp_curve_get_short_data</name>
      <anchor>ga31</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned short *</type>
      <name>stp_curve_get_ushort_data</name>
      <anchor>ga32</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const stp_sequence_t *</type>
      <name>stp_curve_get_sequence</name>
      <anchor>ga33</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_gamma</name>
      <anchor>ga34</anchor>
      <arglist>(stp_curve_t *curve, double f_gamma)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_curve_get_gamma</name>
      <anchor>ga35</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_point</name>
      <anchor>ga36</anchor>
      <arglist>(stp_curve_t *curve, size_t where, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_get_point</name>
      <anchor>ga37</anchor>
      <arglist>(const stp_curve_t *curve, size_t where, double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_interpolate_value</name>
      <anchor>ga38</anchor>
      <arglist>(const stp_curve_t *curve, double where, double *result)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_resample</name>
      <anchor>ga39</anchor>
      <arglist>(stp_curve_t *curve, size_t points)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_rescale</name>
      <anchor>ga40</anchor>
      <arglist>(stp_curve_t *curve, double scale, stp_curve_compose_t mode, stp_curve_bounds_t bounds_mode)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_write</name>
      <anchor>ga41</anchor>
      <arglist>(FILE *file, const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_curve_write_string</name>
      <anchor>ga42</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_stream</name>
      <anchor>ga43</anchor>
      <arglist>(FILE *fp)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_file</name>
      <anchor>ga44</anchor>
      <arglist>(const char *file)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_string</name>
      <anchor>ga45</anchor>
      <arglist>(const char *string)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_compose</name>
      <anchor>ga46</anchor>
      <arglist>(stp_curve_t **retval, stp_curve_t *a, stp_curve_t *b, stp_curve_compose_t mode, int points)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>dither_8h</filename>
    <class kind="struct">stp_dither_matrix_short</class>
    <class kind="struct">stp_dither_matrix_normal</class>
    <class kind="struct">stp_dither_matrix_generic</class>
    <class kind="struct">dither_matrix_impl</class>
    <class kind="struct">stp_dotsize</class>
    <class kind="struct">stp_shade</class>
    <member kind="define">
      <type>#define</type>
      <name>STP_ECOLOR_K</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_ECOLOR_C</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_ECOLOR_M</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_ECOLOR_Y</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_NCOLORS</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_dither_matrix_short</type>
      <name>stp_dither_matrix_short_t</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_dither_matrix_normal</type>
      <name>stp_dither_matrix_normal_t</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_dither_matrix_generic</type>
      <name>stp_dither_matrix_generic_t</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>dither_matrix_impl</type>
      <name>stp_dither_matrix_impl_t</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_dotsize</type>
      <name>stp_dotsize_t</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_shade</type>
      <name>stp_shade_t</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_iterated_init</name>
      <anchor>a11</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, size_t size, size_t exponent, const unsigned *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_shear</name>
      <anchor>a12</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int x_shear, int y_shear)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_init</name>
      <anchor>a13</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int x_size, int y_size, const unsigned int *array, int transpose, int prescaled)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_init_short</name>
      <anchor>a14</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int x_size, int y_size, const unsigned short *array, int transpose, int prescaled)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_dither_matrix_validate_array</name>
      <anchor>a15</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_init_from_dither_array</name>
      <anchor>a16</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, const stp_array_t *array, int transpose)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_destroy</name>
      <anchor>a17</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_clone</name>
      <anchor>a18</anchor>
      <arglist>(const stp_dither_matrix_impl_t *src, stp_dither_matrix_impl_t *dest, int x_offset, int y_offset)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_copy</name>
      <anchor>a19</anchor>
      <arglist>(const stp_dither_matrix_impl_t *src, stp_dither_matrix_impl_t *dest)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_scale_exponentially</name>
      <anchor>a20</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, double exponent)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_set_row</name>
      <anchor>a21</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int y)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_find_standard_dither_array</name>
      <anchor>a22</anchor>
      <arglist>(int x_aspect, int y_aspect)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_dither_list_parameters</name>
      <anchor>a23</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_describe_parameter</name>
      <anchor>a24</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_init</name>
      <anchor>a25</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int out_width, int xdpi, int ydpi)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_iterated_matrix</name>
      <anchor>a26</anchor>
      <arglist>(stp_vars_t *v, size_t edge, size_t iterations, const unsigned *data, int prescaled, int x_shear, int y_shear)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_matrix</name>
      <anchor>a27</anchor>
      <arglist>(stp_vars_t *v, const stp_dither_matrix_generic_t *mat, int transpose, int x_shear, int y_shear)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_matrix_from_dither_array</name>
      <anchor>a28</anchor>
      <arglist>(stp_vars_t *v, const stp_array_t *array, int transpose)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_transition</name>
      <anchor>a29</anchor>
      <arglist>(stp_vars_t *v, double)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_randomizer</name>
      <anchor>a30</anchor>
      <arglist>(stp_vars_t *v, int color, double)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_ink_spread</name>
      <anchor>a31</anchor>
      <arglist>(stp_vars_t *v, int spread)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_adaptive_limit</name>
      <anchor>a32</anchor>
      <arglist>(stp_vars_t *v, double limit)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_dither_get_first_position</name>
      <anchor>a33</anchor>
      <arglist>(stp_vars_t *v, int color, int subchan)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_dither_get_last_position</name>
      <anchor>a34</anchor>
      <arglist>(stp_vars_t *v, int color, int subchan)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_inks_simple</name>
      <anchor>a35</anchor>
      <arglist>(stp_vars_t *v, int color, int nlevels, const double *levels, double density, double darkness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_inks_full</name>
      <anchor>a36</anchor>
      <arglist>(stp_vars_t *v, int color, int nshades, const stp_shade_t *shades, double density, double darkness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_inks</name>
      <anchor>a37</anchor>
      <arglist>(stp_vars_t *v, int color, double density, double darkness, int nshades, const double *svalues, int ndotsizes, const double *dvalues)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_add_channel</name>
      <anchor>a38</anchor>
      <arglist>(stp_vars_t *v, unsigned char *data, unsigned channel, unsigned subchannel)</arglist>
    </member>
    <member kind="function">
      <type>unsigned char *</type>
      <name>stp_dither_get_channel</name>
      <anchor>a39</anchor>
      <arglist>(stp_vars_t *v, unsigned channel, unsigned subchannel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither</name>
      <anchor>a40</anchor>
      <arglist>(stp_vars_t *v, int row, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_internal</name>
      <anchor>a41</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *input, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>gimp-print-intl-internal.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>gimp-print-intl-internal_8h</filename>
    <member kind="define">
      <type>#define</type>
      <name>textdomain</name>
      <anchor>ga0</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>gettext</name>
      <anchor>ga1</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dgettext</name>
      <anchor>ga2</anchor>
      <arglist>(Domain, Message)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dcgettext</name>
      <anchor>ga3</anchor>
      <arglist>(Domain, Message, Type)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>bindtextdomain</name>
      <anchor>ga4</anchor>
      <arglist>(Domain, Directory)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>_</name>
      <anchor>ga5</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>N_</name>
      <anchor>ga6</anchor>
      <arglist>(String)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>gimp-print-intl.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>gimp-print-intl_8h</filename>
    <member kind="define">
      <type>#define</type>
      <name>textdomain</name>
      <anchor>ga0</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>gettext</name>
      <anchor>ga1</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dgettext</name>
      <anchor>ga2</anchor>
      <arglist>(Domain, Message)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dcgettext</name>
      <anchor>ga3</anchor>
      <arglist>(Domain, Message, Type)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>bindtextdomain</name>
      <anchor>ga4</anchor>
      <arglist>(Domain, Directory)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>_</name>
      <anchor>ga5</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>N_</name>
      <anchor>ga6</anchor>
      <arglist>(String)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>gimp-print-module.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>gimp-print-module_8h</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="bit-ops_8h" name="bit-ops.h" local="no">gimp-print/bit-ops.h</includes>
    <includes id="channel_8h" name="channel.h" local="no">gimp-print/channel.h</includes>
    <includes id="color_8h" name="color.h" local="no">gimp-print/color.h</includes>
    <includes id="dither_8h" name="dither.h" local="no">gimp-print/dither.h</includes>
    <includes id="list_8h" name="list.h" local="no">gimp-print/list.h</includes>
    <includes id="module_8h" name="module.h" local="no">gimp-print/module.h</includes>
    <includes id="path_8h" name="path.h" local="no">gimp-print/path.h</includes>
    <includes id="weave_8h" name="weave.h" local="no">gimp-print/weave.h</includes>
    <includes id="xml_8h" name="xml.h" local="no">gimp-print/xml.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>STP_MODULE</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>gimp-print-version.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>gimp-print-version_8h</filename>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_MAJOR_VERSION</name>
      <anchor>ga7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_MINOR_VERSION</name>
      <anchor>ga8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_MICRO_VERSION</name>
      <anchor>ga9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_CURRENT_INTERFACE</name>
      <anchor>ga10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_BINARY_AGE</name>
      <anchor>ga11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_INTERFACE_AGE</name>
      <anchor>ga12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_CHECK_VERSION</name>
      <anchor>ga13</anchor>
      <arglist>(major, minor, micro)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_check_version</name>
      <anchor>ga6</anchor>
      <arglist>(unsigned int required_major, unsigned int required_minor, unsigned int required_micro)</arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_major_version</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_minor_version</name>
      <anchor>ga1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_micro_version</name>
      <anchor>ga2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_current_interface</name>
      <anchor>ga3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_binary_age</name>
      <anchor>ga4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_interface_age</name>
      <anchor>ga5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>gimp-print.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>gimp-print_8h</filename>
    <includes id="array_8h" name="array.h" local="no">gimp-print/array.h</includes>
    <includes id="curve_8h" name="curve.h" local="no">gimp-print/curve.h</includes>
    <includes id="gimp-print-version_8h" name="gimp-print-version.h" local="no">gimp-print/gimp-print-version.h</includes>
    <includes id="image_8h" name="image.h" local="no">gimp-print/image.h</includes>
    <includes id="paper_8h" name="paper.h" local="no">gimp-print/paper.h</includes>
    <includes id="printers_8h" name="printers.h" local="no">gimp-print/printers.h</includes>
    <includes id="sequence_8h" name="sequence.h" local="no">gimp-print/sequence.h</includes>
    <includes id="string-list_8h" name="string-list.h" local="no">gimp-print/string-list.h</includes>
    <includes id="include_2gimp-print_2util_8h" name="util.h" local="no">gimp-print/util.h</includes>
    <includes id="vars_8h" name="vars.h" local="no">gimp-print/vars.h</includes>
  </compound>
  <compound kind="file">
    <name>image.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>image_8h</filename>
    <class kind="struct">stp_image</class>
    <member kind="define">
      <type>#define</type>
      <name>STP_CHANNEL_LIMIT</name>
      <anchor>ga8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_image</type>
      <name>stp_image_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_image_status_t</name>
      <anchor>ga9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_IMAGE_STATUS_OK</name>
      <anchor>gga9a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_IMAGE_STATUS_ABORT</name>
      <anchor>gga9a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_init</name>
      <anchor>ga1</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_reset</name>
      <anchor>ga2</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_image_width</name>
      <anchor>ga3</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_image_height</name>
      <anchor>ga4</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>stp_image_status_t</type>
      <name>stp_image_get_row</name>
      <anchor>ga5</anchor>
      <arglist>(stp_image_t *image, unsigned char *data, size_t limit, int row)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_image_get_appname</name>
      <anchor>ga6</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_conclude</name>
      <anchor>ga7</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>list.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>list_8h</filename>
    <member kind="typedef">
      <type>stp_list_item</type>
      <name>stp_list_item_t</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_list</type>
      <name>stp_list_t</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>stp_node_freefunc</name>
      <anchor>a2</anchor>
      <arglist>)(void *)</arglist>
    </member>
    <member kind="typedef">
      <type>void *(*</type>
      <name>stp_node_copyfunc</name>
      <anchor>a3</anchor>
      <arglist>)(const void *)</arglist>
    </member>
    <member kind="typedef">
      <type>const char *(*</type>
      <name>stp_node_namefunc</name>
      <anchor>a4</anchor>
      <arglist>)(const void *)</arglist>
    </member>
    <member kind="typedef">
      <type>int(*</type>
      <name>stp_node_sortfunc</name>
      <anchor>a5</anchor>
      <arglist>)(const void *, const void *)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_node_free_data</name>
      <anchor>a6</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_list_create</name>
      <anchor>a7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_list_copy</name>
      <anchor>a8</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_destroy</name>
      <anchor>a9</anchor>
      <arglist>(stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_start</name>
      <anchor>a10</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_end</name>
      <anchor>a11</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_index</name>
      <anchor>a12</anchor>
      <arglist>(const stp_list_t *list, int idx)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_name</name>
      <anchor>a13</anchor>
      <arglist>(const stp_list_t *list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_long_name</name>
      <anchor>a14</anchor>
      <arglist>(const stp_list_t *list, const char *long_name)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_get_length</name>
      <anchor>a15</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_freefunc</name>
      <anchor>a16</anchor>
      <arglist>(stp_list_t *list, stp_node_freefunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_freefunc</type>
      <name>stp_list_get_freefunc</name>
      <anchor>a17</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_copyfunc</name>
      <anchor>a18</anchor>
      <arglist>(stp_list_t *list, stp_node_copyfunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_copyfunc</type>
      <name>stp_list_get_copyfunc</name>
      <anchor>a19</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_namefunc</name>
      <anchor>a20</anchor>
      <arglist>(stp_list_t *list, stp_node_namefunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_namefunc</type>
      <name>stp_list_get_namefunc</name>
      <anchor>a21</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_long_namefunc</name>
      <anchor>a22</anchor>
      <arglist>(stp_list_t *list, stp_node_namefunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_namefunc</type>
      <name>stp_list_get_long_namefunc</name>
      <anchor>a23</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_sortfunc</name>
      <anchor>a24</anchor>
      <arglist>(stp_list_t *list, stp_node_sortfunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_sortfunc</type>
      <name>stp_list_get_sortfunc</name>
      <anchor>a25</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_item_create</name>
      <anchor>a26</anchor>
      <arglist>(stp_list_t *list, stp_list_item_t *next, const void *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_item_destroy</name>
      <anchor>a27</anchor>
      <arglist>(stp_list_t *list, stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_item_prev</name>
      <anchor>a28</anchor>
      <arglist>(const stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_item_next</name>
      <anchor>a29</anchor>
      <arglist>(const stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_list_item_get_data</name>
      <anchor>a30</anchor>
      <arglist>(const stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_item_set_data</name>
      <anchor>a31</anchor>
      <arglist>(stp_list_item_t *item, void *data)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>module.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>module_8h</filename>
    <includes id="list_8h" name="list.h" local="no">gimp-print/list.h</includes>
    <class kind="struct">stp_module_version</class>
    <class kind="struct">stp_module</class>
    <member kind="typedef">
      <type>stp_module_version</type>
      <name>stp_module_version_t</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_module</type>
      <name>stp_module_t</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_module_class_t</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MODULE_CLASS_INVALID</name>
      <anchor>a13a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MODULE_CLASS_MISC</name>
      <anchor>a13a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MODULE_CLASS_FAMILY</name>
      <anchor>a13a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MODULE_CLASS_COLOR</name>
      <anchor>a13a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MODULE_CLASS_DITHER</name>
      <anchor>a13a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_load</name>
      <anchor>a7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_exit</name>
      <anchor>a8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_open</name>
      <anchor>a9</anchor>
      <arglist>(const char *modulename)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_init</name>
      <anchor>a10</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_close</name>
      <anchor>a11</anchor>
      <arglist>(stp_list_item_t *module)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_module_get_class</name>
      <anchor>a12</anchor>
      <arglist>(stp_module_class_t class)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>mxml.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>mxml_8h</filename>
    <class kind="struct">stp_mxml_attr_s</class>
    <class kind="struct">stp_mxml_value_s</class>
    <class kind="struct">stp_mxml_text_s</class>
    <class kind="union">stp_mxml_value_u</class>
    <class kind="struct">stp_mxml_node_s</class>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_WRAP</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_TAB</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_NO_CALLBACK</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_NO_PARENT</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_DESCEND</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_NO_DESCEND</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_DESCEND_FIRST</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_WS_BEFORE_OPEN</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_WS_AFTER_OPEN</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_WS_BEFORE_CLOSE</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_WS_AFTER_CLOSE</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_ADD_BEFORE</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_ADD_AFTER</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_MXML_ADD_TO_PARENT</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum stp_mxml_type_e</type>
      <name>stp_mxml_type_t</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_mxml_attr_s</type>
      <name>stp_mxml_attr_t</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_mxml_value_s</type>
      <name>stp_mxml_element_t</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_mxml_text_s</type>
      <name>stp_mxml_text_t</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_mxml_value_u</type>
      <name>stp_mxml_value_t</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_mxml_node_s</type>
      <name>stp_mxml_node_t</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_mxml_type_e</name>
      <anchor>a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MXML_ELEMENT</name>
      <anchor>a43a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MXML_INTEGER</name>
      <anchor>a43a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MXML_OPAQUE</name>
      <anchor>a43a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MXML_REAL</name>
      <anchor>a43a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_MXML_TEXT</name>
      <anchor>a43a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlAdd</name>
      <anchor>a25</anchor>
      <arglist>(stp_mxml_node_t *parent, int where, stp_mxml_node_t *child, stp_mxml_node_t *node)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlDelete</name>
      <anchor>a26</anchor>
      <arglist>(stp_mxml_node_t *node)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_mxmlElementGetAttr</name>
      <anchor>a27</anchor>
      <arglist>(stp_mxml_node_t *node, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlElementSetAttr</name>
      <anchor>a28</anchor>
      <arglist>(stp_mxml_node_t *node, const char *name, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlFindElement</name>
      <anchor>a29</anchor>
      <arglist>(stp_mxml_node_t *node, stp_mxml_node_t *top, const char *name, const char *attr, const char *value, int descend)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlLoadFile</name>
      <anchor>a30</anchor>
      <arglist>(stp_mxml_node_t *top, FILE *fp, stp_mxml_type_t(*cb)(stp_mxml_node_t *))</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlLoadString</name>
      <anchor>a31</anchor>
      <arglist>(stp_mxml_node_t *top, const char *s, stp_mxml_type_t(*cb)(stp_mxml_node_t *))</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewElement</name>
      <anchor>a32</anchor>
      <arglist>(stp_mxml_node_t *parent, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewInteger</name>
      <anchor>a33</anchor>
      <arglist>(stp_mxml_node_t *parent, int integer)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewOpaque</name>
      <anchor>a34</anchor>
      <arglist>(stp_mxml_node_t *parent, const char *opaque)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewReal</name>
      <anchor>a35</anchor>
      <arglist>(stp_mxml_node_t *parent, double real)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewText</name>
      <anchor>a36</anchor>
      <arglist>(stp_mxml_node_t *parent, int whitespace, const char *string)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlRemove</name>
      <anchor>a37</anchor>
      <arglist>(stp_mxml_node_t *node)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_mxmlSaveAllocString</name>
      <anchor>a38</anchor>
      <arglist>(stp_mxml_node_t *node, int(*cb)(stp_mxml_node_t *, int))</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_mxmlSaveFile</name>
      <anchor>a39</anchor>
      <arglist>(stp_mxml_node_t *node, FILE *fp, int(*cb)(stp_mxml_node_t *, int))</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_mxmlSaveString</name>
      <anchor>a40</anchor>
      <arglist>(stp_mxml_node_t *node, char *buffer, int bufsize, int(*cb)(stp_mxml_node_t *, int))</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlWalkNext</name>
      <anchor>a41</anchor>
      <arglist>(stp_mxml_node_t *node, stp_mxml_node_t *top, int descend)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlWalkPrev</name>
      <anchor>a42</anchor>
      <arglist>(stp_mxml_node_t *node, stp_mxml_node_t *top, int descend)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>paper.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>paper_8h</filename>
    <includes id="vars_8h" name="vars.h" local="no">gimp-print/vars.h</includes>
    <class kind="struct">stp_papersize_t</class>
    <member kind="enumeration">
      <name>stp_papersize_unit_t</name>
      <anchor>ga5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_ENGLISH_STANDARD</name>
      <anchor>gga5a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_METRIC_STANDARD</name>
      <anchor>gga5a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_ENGLISH_EXTENDED</name>
      <anchor>gga5a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_METRIC_EXTENDED</name>
      <anchor>gga5a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_known_papersizes</name>
      <anchor>ga0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_name</name>
      <anchor>ga1</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_size</name>
      <anchor>ga2</anchor>
      <arglist>(int length, int width)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_index</name>
      <anchor>ga3</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_default_media_size</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>path.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>path_8h</filename>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_path_search</name>
      <anchor>a0</anchor>
      <arglist>(stp_list_t *dirlist, const char *suffix)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_path_split</name>
      <anchor>a1</anchor>
      <arglist>(stp_list_t *list, const char *path)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>printers.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>printers_8h</filename>
    <includes id="list_8h" name="list.h" local="no">gimp-print/list.h</includes>
    <includes id="vars_8h" name="vars.h" local="no">gimp-print/vars.h</includes>
    <class kind="struct">stp_printfuncs_t</class>
    <class kind="struct">stp_family</class>
    <member kind="typedef">
      <type>stp_printer</type>
      <name>stp_printer_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_family</type>
      <name>stp_family_t</name>
      <anchor>ga1</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_printer_model_count</name>
      <anchor>ga2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_index</name>
      <anchor>ga3</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_long_name</name>
      <anchor>ga4</anchor>
      <arglist>(const char *long_name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_driver</name>
      <anchor>ga5</anchor>
      <arglist>(const char *driver)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_printer_index_by_driver</name>
      <anchor>ga7</anchor>
      <arglist>(const char *driver)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_long_name</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_driver</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_family</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_manufacturer</name>
      <anchor>ga11</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_printer_get_model</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const stp_vars_t *</type>
      <name>stp_printer_get_defaults</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_printer_defaults</name>
      <anchor>ga14</anchor>
      <arglist>(stp_vars_t *v, const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_print</name>
      <anchor>ga15</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_start_job</name>
      <anchor>ga16</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_end_job</name>
      <anchor>ga17</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_model_id</name>
      <anchor>ga18</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_verify_printer_params</name>
      <anchor>ga19</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_family_register</name>
      <anchor>ga20</anchor>
      <arglist>(stp_list_t *family)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_family_unregister</name>
      <anchor>ga21</anchor>
      <arglist>(stp_list_t *family)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_initialize_printer_defaults</name>
      <anchor>ga22</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_printer_list_parameters</name>
      <anchor>ga23</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_printer_describe_parameter</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_describe_output</name>
      <anchor>ga25</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sequence.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>sequence_8h</filename>
    <member kind="typedef">
      <type>stp_sequence</type>
      <name>stp_sequence_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create</name>
      <anchor>ga1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_destroy</name>
      <anchor>ga2</anchor>
      <arglist>(stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_copy</name>
      <anchor>ga3</anchor>
      <arglist>(stp_sequence_t *dest, const stp_sequence_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create_copy</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_bounds</name>
      <anchor>ga5</anchor>
      <arglist>(stp_sequence_t *sequence, double low, double high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_bounds</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_sequence_t *sequence, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_range</name>
      <anchor>ga7</anchor>
      <arglist>(const stp_sequence_t *sequence, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_size</name>
      <anchor>ga8</anchor>
      <arglist>(stp_sequence_t *sequence, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_sequence_get_size</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_data</name>
      <anchor>ga10</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_subrange</name>
      <anchor>ga11</anchor>
      <arglist>(stp_sequence_t *sequence, size_t where, size_t size, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_data</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *size, const double **data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_point</name>
      <anchor>ga13</anchor>
      <arglist>(stp_sequence_t *sequence, size_t where, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_get_point</name>
      <anchor>ga14</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t where, double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_float_data</name>
      <anchor>ga15</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const float *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_long_data</name>
      <anchor>ga16</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_ulong_data</name>
      <anchor>ga17</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const unsigned long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_int_data</name>
      <anchor>ga18</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_uint_data</name>
      <anchor>ga19</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const unsigned int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_short_data</name>
      <anchor>ga20</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const short *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_ushort_data</name>
      <anchor>ga21</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const unsigned short *data)</arglist>
    </member>
    <member kind="function">
      <type>const float *</type>
      <name>stp_sequence_get_float_data</name>
      <anchor>ga22</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const long *</type>
      <name>stp_sequence_get_long_data</name>
      <anchor>ga23</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned long *</type>
      <name>stp_sequence_get_ulong_data</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const int *</type>
      <name>stp_sequence_get_int_data</name>
      <anchor>ga25</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned int *</type>
      <name>stp_sequence_get_uint_data</name>
      <anchor>ga26</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const short *</type>
      <name>stp_sequence_get_short_data</name>
      <anchor>ga27</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned short *</type>
      <name>stp_sequence_get_ushort_data</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>string-list.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>string-list_8h</filename>
    <class kind="struct">stp_param_string_t</class>
    <member kind="typedef">
      <type>stp_string_list</type>
      <name>stp_string_list_t</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_string_list_t *</type>
      <name>stp_string_list_create</name>
      <anchor>a1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_string_list_destroy</name>
      <anchor>a2</anchor>
      <arglist>(stp_string_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_param_string_t *</type>
      <name>stp_string_list_param</name>
      <anchor>a3</anchor>
      <arglist>(const stp_string_list_t *list, size_t element)</arglist>
    </member>
    <member kind="function">
      <type>stp_param_string_t *</type>
      <name>stp_string_list_find</name>
      <anchor>a4</anchor>
      <arglist>(const stp_string_list_t *list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_string_list_count</name>
      <anchor>a5</anchor>
      <arglist>(const stp_string_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_string_list_t *</type>
      <name>stp_string_list_create_copy</name>
      <anchor>a6</anchor>
      <arglist>(const stp_string_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_string_list_add_string</name>
      <anchor>a7</anchor>
      <arglist>(stp_string_list_t *list, const char *name, const char *text)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_string_list_remove_string</name>
      <anchor>a8</anchor>
      <arglist>(stp_string_list_t *list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>stp_string_list_t *</type>
      <name>stp_string_list_create_from_params</name>
      <anchor>a9</anchor>
      <arglist>(const stp_param_string_t *list, size_t count)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_string_list_is_present</name>
      <anchor>a10</anchor>
      <arglist>(const stp_string_list_t *list, const char *value)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>util.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>include_2gimp-print_2util_8h</filename>
    <includes id="curve_8h" name="curve.h" local="no">gimp-print/curve.h</includes>
    <includes id="vars_8h" name="vars.h" local="no">gimp-print/vars.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_LUT</name>
      <anchor>ga31</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_COLORFUNC</name>
      <anchor>ga32</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_INK</name>
      <anchor>ga33</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PS</name>
      <anchor>ga34</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PCL</name>
      <anchor>ga35</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_ESCP2</name>
      <anchor>ga36</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_CANON</name>
      <anchor>ga37</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_LEXMARK</name>
      <anchor>ga38</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_WEAVE_PARAMS</name>
      <anchor>ga39</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_ROWS</name>
      <anchor>ga40</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_MARK_FILE</name>
      <anchor>ga41</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_LIST</name>
      <anchor>ga42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_MODULE</name>
      <anchor>ga43</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PATH</name>
      <anchor>ga44</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PAPER</name>
      <anchor>ga45</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PRINTERS</name>
      <anchor>ga46</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_XML</name>
      <anchor>ga47</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_VARS</name>
      <anchor>ga48</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_OLYMPUS</name>
      <anchor>ga49</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_CURVE</name>
      <anchor>ga50</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_CURVE_ERRORS</name>
      <anchor>ga51</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_SAFE_FREE</name>
      <anchor>ga52</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_init</name>
      <anchor>ga0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_set_output_codeset</name>
      <anchor>ga1</anchor>
      <arglist>(const char *codeset)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_read_and_compose_curves</name>
      <anchor>ga2</anchor>
      <arglist>(const char *s1, const char *s2, stp_curve_compose_t comp, size_t piecewise_point_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_abort</name>
      <anchor>ga3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_prune_inactive_options</name>
      <anchor>ga4</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_zprintf</name>
      <anchor>ga5</anchor>
      <arglist>(const stp_vars_t *v, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_zfwrite</name>
      <anchor>ga6</anchor>
      <arglist>(const char *buf, size_t bytes, size_t nitems, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_putc</name>
      <anchor>ga7</anchor>
      <arglist>(int ch, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put16_le</name>
      <anchor>ga8</anchor>
      <arglist>(unsigned short sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put16_be</name>
      <anchor>ga9</anchor>
      <arglist>(unsigned short sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put32_le</name>
      <anchor>ga10</anchor>
      <arglist>(unsigned int sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put32_be</name>
      <anchor>ga11</anchor>
      <arglist>(unsigned int sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_puts</name>
      <anchor>ga12</anchor>
      <arglist>(const char *s, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_send_command</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_vars_t *v, const char *command, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_erputc</name>
      <anchor>ga14</anchor>
      <arglist>(int ch)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_eprintf</name>
      <anchor>ga15</anchor>
      <arglist>(const stp_vars_t *v, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_erprintf</name>
      <anchor>ga16</anchor>
      <arglist>(const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_asprintf</name>
      <anchor>ga17</anchor>
      <arglist>(char **strp, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_catprintf</name>
      <anchor>ga18</anchor>
      <arglist>(char **strp, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>unsigned long</type>
      <name>stp_get_debug_level</name>
      <anchor>ga19</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dprintf</name>
      <anchor>ga20</anchor>
      <arglist>(unsigned long level, const stp_vars_t *v, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_deprintf</name>
      <anchor>ga21</anchor>
      <arglist>(unsigned long level, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_init_debug_messages</name>
      <anchor>ga22</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_flush_debug_messages</name>
      <anchor>ga23</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_malloc</name>
      <anchor>ga24</anchor>
      <arglist>(size_t)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_zalloc</name>
      <anchor>ga25</anchor>
      <arglist>(size_t)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_realloc</name>
      <anchor>ga26</anchor>
      <arglist>(void *ptr, size_t)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_free</name>
      <anchor>ga27</anchor>
      <arglist>(void *ptr)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_strlen</name>
      <anchor>ga28</anchor>
      <arglist>(const char *s)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_strndup</name>
      <anchor>ga29</anchor>
      <arglist>(const char *s, int n)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_strdup</name>
      <anchor>ga30</anchor>
      <arglist>(const char *s)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>util.h</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>src_2main_2util_8h</filename>
    <member kind="define">
      <type>#define</type>
      <name>__attribute__</name>
      <anchor>a0</anchor>
      <arglist>(ignore)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_paper</name>
      <anchor>ga0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_dither</name>
      <anchor>ga1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_printer</name>
      <anchor>ga2</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>vars.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>vars_8h</filename>
    <includes id="array_8h" name="array.h" local="no">gimp-print/array.h</includes>
    <includes id="curve_8h" name="curve.h" local="no">gimp-print/curve.h</includes>
    <includes id="string-list_8h" name="string-list.h" local="no">gimp-print/string-list.h</includes>
    <class kind="struct">stp_raw_t</class>
    <class kind="struct">stp_double_bound_t</class>
    <class kind="struct">stp_int_bound_t</class>
    <class kind="struct">stp_parameter_t</class>
    <member kind="typedef">
      <type>stp_vars</type>
      <name>stp_vars_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void *</type>
      <name>stp_parameter_list_t</name>
      <anchor>ga1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>const void *</type>
      <name>stp_const_parameter_list_t</name>
      <anchor>ga2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>stp_outfunc_t</name>
      <anchor>ga3</anchor>
      <arglist>)(void *data, const char *buffer, size_t bytes)</arglist>
    </member>
    <member kind="typedef">
      <type>void *(*</type>
      <name>stp_copy_data_func_t</name>
      <anchor>ga4</anchor>
      <arglist>)(void *)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>stp_free_data_func_t</name>
      <anchor>ga5</anchor>
      <arglist>)(void *)</arglist>
    </member>
    <member kind="typedef">
      <type>stp_compdata</type>
      <name>compdata_t</name>
      <anchor>ga6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_type_t</name>
      <anchor>ga132</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_STRING_LIST</name>
      <anchor>gga132a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_INT</name>
      <anchor>gga132a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_BOOLEAN</name>
      <anchor>gga132a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_DOUBLE</name>
      <anchor>gga132a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_CURVE</name>
      <anchor>gga132a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_FILE</name>
      <anchor>gga132a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_RAW</name>
      <anchor>gga132a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_ARRAY</name>
      <anchor>gga132a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_DIMENSION</name>
      <anchor>gga132a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_INVALID</name>
      <anchor>gga132a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_class_t</name>
      <anchor>ga133</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_FEATURE</name>
      <anchor>gga133a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_OUTPUT</name>
      <anchor>gga133a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_CORE</name>
      <anchor>gga133a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_INVALID</name>
      <anchor>gga133a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_level_t</name>
      <anchor>ga134</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_BASIC</name>
      <anchor>gga134a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED</name>
      <anchor>gga134a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED1</name>
      <anchor>gga134a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED2</name>
      <anchor>gga134a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED3</name>
      <anchor>gga134a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED4</name>
      <anchor>gga134a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_INTERNAL</name>
      <anchor>gga134a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_EXTERNAL</name>
      <anchor>gga134a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_INVALID</name>
      <anchor>gga134a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_activity_t</name>
      <anchor>ga135</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_INACTIVE</name>
      <anchor>gga135a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_DEFAULTED</name>
      <anchor>gga135a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_ACTIVE</name>
      <anchor>gga135a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_verify_t</name>
      <anchor>ga136</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PARAMETER_BAD</name>
      <anchor>gga136a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PARAMETER_OK</name>
      <anchor>gga136a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PARAMETER_INACTIVE</name>
      <anchor>gga136a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_vars_t *</type>
      <name>stp_vars_create</name>
      <anchor>ga7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_vars_copy</name>
      <anchor>ga8</anchor>
      <arglist>(stp_vars_t *dest, const stp_vars_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_vars_t *</type>
      <name>stp_vars_create_copy</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_vars_t *source)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_vars_destroy</name>
      <anchor>ga10</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_driver</name>
      <anchor>ga11</anchor>
      <arglist>(stp_vars_t *v, const char *val)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_driver_n</name>
      <anchor>ga12</anchor>
      <arglist>(stp_vars_t *v, const char *val, int bytes)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_driver</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_color_conversion</name>
      <anchor>ga14</anchor>
      <arglist>(stp_vars_t *v, const char *val)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_color_conversion_n</name>
      <anchor>ga15</anchor>
      <arglist>(stp_vars_t *v, const char *val, int bytes)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_color_conversion</name>
      <anchor>ga16</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_left</name>
      <anchor>ga17</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_left</name>
      <anchor>ga18</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_top</name>
      <anchor>ga19</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_top</name>
      <anchor>ga20</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_width</name>
      <anchor>ga21</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_width</name>
      <anchor>ga22</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_height</name>
      <anchor>ga23</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_height</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_page_width</name>
      <anchor>ga25</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_page_width</name>
      <anchor>ga26</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_page_height</name>
      <anchor>ga27</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_page_height</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_outfunc</name>
      <anchor>ga29</anchor>
      <arglist>(stp_vars_t *v, stp_outfunc_t val)</arglist>
    </member>
    <member kind="function">
      <type>stp_outfunc_t</type>
      <name>stp_get_outfunc</name>
      <anchor>ga30</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_errfunc</name>
      <anchor>ga31</anchor>
      <arglist>(stp_vars_t *v, stp_outfunc_t val)</arglist>
    </member>
    <member kind="function">
      <type>stp_outfunc_t</type>
      <name>stp_get_errfunc</name>
      <anchor>ga32</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_outdata</name>
      <anchor>ga33</anchor>
      <arglist>(stp_vars_t *v, void *val)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_get_outdata</name>
      <anchor>ga34</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_errdata</name>
      <anchor>ga35</anchor>
      <arglist>(stp_vars_t *v, void *val)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_get_errdata</name>
      <anchor>ga36</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_merge_printvars</name>
      <anchor>ga37</anchor>
      <arglist>(stp_vars_t *user, const stp_vars_t *print)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_get_parameter_list</name>
      <anchor>ga38</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_parameter_list_count</name>
      <anchor>ga39</anchor>
      <arglist>(stp_const_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_find</name>
      <anchor>ga40</anchor>
      <arglist>(stp_const_parameter_list_t list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_list_param</name>
      <anchor>ga41</anchor>
      <arglist>(stp_const_parameter_list_t list, size_t item)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_destroy</name>
      <anchor>ga42</anchor>
      <arglist>(stp_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_parameter_list_create</name>
      <anchor>ga43</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_add_param</name>
      <anchor>ga44</anchor>
      <arglist>(stp_parameter_list_t list, const stp_parameter_t *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_parameter_list_copy</name>
      <anchor>ga45</anchor>
      <arglist>(stp_const_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_append</name>
      <anchor>ga46</anchor>
      <arglist>(stp_parameter_list_t list, stp_const_parameter_list_t append)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_describe_parameter</name>
      <anchor>ga47</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_description_destroy</name>
      <anchor>ga48</anchor>
      <arglist>(stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_find_in_settings</name>
      <anchor>ga49</anchor>
      <arglist>(const stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter</name>
      <anchor>ga50</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter_n</name>
      <anchor>ga51</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter</name>
      <anchor>ga52</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter_n</name>
      <anchor>ga53</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_float_parameter</name>
      <anchor>ga54</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_int_parameter</name>
      <anchor>ga55</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_dimension_parameter</name>
      <anchor>ga56</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_boolean_parameter</name>
      <anchor>ga57</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_curve_parameter</name>
      <anchor>ga58</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_curve_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_array_parameter</name>
      <anchor>ga59</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_array_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_raw_parameter</name>
      <anchor>ga60</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const void *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_scale_float_parameter</name>
      <anchor>ga61</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double scale)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_string_parameter</name>
      <anchor>ga62</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_string_parameter_n</name>
      <anchor>ga63</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_file_parameter</name>
      <anchor>ga64</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_file_parameter_n</name>
      <anchor>ga65</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_float_parameter</name>
      <anchor>ga66</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_int_parameter</name>
      <anchor>ga67</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_dimension_parameter</name>
      <anchor>ga68</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_boolean_parameter</name>
      <anchor>ga69</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_curve_parameter</name>
      <anchor>ga70</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_curve_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_array_parameter</name>
      <anchor>ga71</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_array_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_raw_parameter</name>
      <anchor>ga72</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const void *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_string_parameter</name>
      <anchor>ga73</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_file_parameter</name>
      <anchor>ga74</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_get_float_parameter</name>
      <anchor>ga75</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_int_parameter</name>
      <anchor>ga76</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_dimension_parameter</name>
      <anchor>ga77</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_boolean_parameter</name>
      <anchor>ga78</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_curve_t *</type>
      <name>stp_get_curve_parameter</name>
      <anchor>ga79</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_array_t *</type>
      <name>stp_get_array_parameter</name>
      <anchor>ga80</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_raw_t *</type>
      <name>stp_get_raw_parameter</name>
      <anchor>ga81</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_string_parameter</name>
      <anchor>ga82</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_file_parameter</name>
      <anchor>ga83</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_float_parameter</name>
      <anchor>ga84</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_int_parameter</name>
      <anchor>ga85</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_dimension_parameter</name>
      <anchor>ga86</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_boolean_parameter</name>
      <anchor>ga87</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_curve_parameter</name>
      <anchor>ga88</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_array_parameter</name>
      <anchor>ga89</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_raw_parameter</name>
      <anchor>ga90</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter_active</name>
      <anchor>ga91</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter_active</name>
      <anchor>ga92</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_float_parameter_active</name>
      <anchor>ga93</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_int_parameter_active</name>
      <anchor>ga94</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_dimension_parameter_active</name>
      <anchor>ga95</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_boolean_parameter_active</name>
      <anchor>ga96</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_curve_parameter_active</name>
      <anchor>ga97</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_array_parameter_active</name>
      <anchor>ga98</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_raw_parameter_active</name>
      <anchor>ga99</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_string_parameter</name>
      <anchor>ga100</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_file_parameter</name>
      <anchor>ga101</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_float_parameter</name>
      <anchor>ga102</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_int_parameter</name>
      <anchor>ga103</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_dimension_parameter</name>
      <anchor>ga104</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_boolean_parameter</name>
      <anchor>ga105</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_curve_parameter</name>
      <anchor>ga106</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_array_parameter</name>
      <anchor>ga107</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_raw_parameter</name>
      <anchor>ga108</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_string_parameter_active</name>
      <anchor>ga109</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_file_parameter_active</name>
      <anchor>ga110</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_float_parameter_active</name>
      <anchor>ga111</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_int_parameter_active</name>
      <anchor>ga112</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_dimension_parameter_active</name>
      <anchor>ga113</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_boolean_parameter_active</name>
      <anchor>ga114</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_curve_parameter_active</name>
      <anchor>ga115</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_array_parameter_active</name>
      <anchor>ga116</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_raw_parameter_active</name>
      <anchor>ga117</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_media_size</name>
      <anchor>ga118</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_imageable_area</name>
      <anchor>ga119</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_size_limit</name>
      <anchor>ga120</anchor>
      <arglist>(const stp_vars_t *v, int *max_width, int *max_height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_describe_resolution</name>
      <anchor>ga121</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_verify</name>
      <anchor>ga122</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>const stp_vars_t *</type>
      <name>stp_default_settings</name>
      <anchor>ga123</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_allocate_component_data</name>
      <anchor>ga124</anchor>
      <arglist>(stp_vars_t *v, const char *name, stp_copy_data_func_t copyfunc, stp_free_data_func_t freefunc, void *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_destroy_component_data</name>
      <anchor>ga125</anchor>
      <arglist>(stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_get_component_data</name>
      <anchor>ga126</anchor>
      <arglist>(const stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_verify_t</type>
      <name>stp_verify_parameter</name>
      <anchor>ga127</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, int quiet)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_verified</name>
      <anchor>ga128</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_verified</name>
      <anchor>ga129</anchor>
      <arglist>(stp_vars_t *v, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_copy_options</name>
      <anchor>ga130</anchor>
      <arglist>(stp_vars_t *vd, const stp_vars_t *vs)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_fill_parameter_settings</name>
      <anchor>ga131</anchor>
      <arglist>(stp_parameter_t *desc, const stp_parameter_t *param)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>weave.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>weave_8h</filename>
    <class kind="struct">stp_weave_t</class>
    <class kind="struct">stp_pass_t</class>
    <class kind="struct">stp_lineoff_t</class>
    <class kind="struct">stp_lineactive_t</class>
    <class kind="struct">stp_linecount_t</class>
    <class kind="struct">stp_linebufs_t</class>
    <class kind="struct">stp_linebounds_t</class>
    <member kind="define">
      <type>#define</type>
      <name>STP_MAX_WEAVE</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>stp_packfunc</name>
      <anchor>a1</anchor>
      <arglist>(stp_vars_t *v, const unsigned char *line, int height, unsigned char *comp_buf, unsigned char **comp_ptr, int *first, int *last)</arglist>
    </member>
    <member kind="typedef">
      <type>void</type>
      <name>stp_fillfunc</name>
      <anchor>a2</anchor>
      <arglist>(stp_vars_t *v, int row, int subpass, int width, int missingstartrows, int color)</arglist>
    </member>
    <member kind="typedef">
      <type>void</type>
      <name>stp_flushfunc</name>
      <anchor>a3</anchor>
      <arglist>(stp_vars_t *v, int passno, int vertical_subpass)</arglist>
    </member>
    <member kind="typedef">
      <type>int</type>
      <name>stp_compute_linewidth_func</name>
      <anchor>a4</anchor>
      <arglist>(stp_vars_t *v, int n)</arglist>
    </member>
    <member kind="enumeration">
      <name>stp_weave_strategy_t</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_WEAVE_ZIGZAG</name>
      <anchor>a26a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_WEAVE_ASCENDING</name>
      <anchor>a26a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_WEAVE_DESCENDING</name>
      <anchor>a26a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_WEAVE_ASCENDING_2X</name>
      <anchor>a26a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_WEAVE_STAGGERED</name>
      <anchor>a26a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_WEAVE_ASCENDING_3X</name>
      <anchor>a26a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_initialize_weave</name>
      <anchor>a17</anchor>
      <arglist>(stp_vars_t *v, int jets, int separation, int oversample, int horizontal, int vertical, int ncolors, int bitwidth, int linewidth, int line_count, int first_line, int page_height, const int *head_offset, stp_weave_strategy_t, stp_flushfunc, stp_fillfunc, stp_packfunc, stp_compute_linewidth_func)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_flush_all</name>
      <anchor>a18</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_write_weave</name>
      <anchor>a19</anchor>
      <arglist>(stp_vars_t *v, unsigned char *const cols[])</arglist>
    </member>
    <member kind="function">
      <type>stp_lineoff_t *</type>
      <name>stp_get_lineoffsets_by_pass</name>
      <anchor>a20</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>stp_lineactive_t *</type>
      <name>stp_get_lineactive_by_pass</name>
      <anchor>a21</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>stp_linecount_t *</type>
      <name>stp_get_linecount_by_pass</name>
      <anchor>a22</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>const stp_linebufs_t *</type>
      <name>stp_get_linebases_by_pass</name>
      <anchor>a23</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>stp_pass_t *</type>
      <name>stp_get_pass_by_pass</name>
      <anchor>a24</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_weave_parameters_by_row</name>
      <anchor>a25</anchor>
      <arglist>(const stp_vars_t *v, int row, int vertical_subpass, stp_weave_t *w)</arglist>
    </member>
    <member kind="variable">
      <type>stp_packfunc</type>
      <name>stp_pack_tiff</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_packfunc</type>
      <name>stp_pack_uncompressed</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_fillfunc</type>
      <name>stp_fill_tiff</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_fillfunc</type>
      <name>stp_fill_uncompressed</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_compute_linewidth_func</type>
      <name>stp_compute_tiff_linewidth</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_compute_linewidth_func</type>
      <name>stp_compute_uncompressed_linewidth</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>xml.h</name>
    <path>/home/rlk/sandbox/print-4.3/include/gimp-print/</path>
    <filename>xml_8h</filename>
    <includes id="mxml_8h" name="mxml.h" local="no">gimp-print/mxml.h</includes>
    <member kind="typedef">
      <type>int(*</type>
      <name>stp_xml_parse_func</name>
      <anchor>a0</anchor>
      <arglist>)(stp_mxml_node_t *node, const char *file)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_register_xml_parser</name>
      <anchor>a1</anchor>
      <arglist>(const char *name, stp_xml_parse_func parse_func)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unregister_xml_parser</name>
      <anchor>a2</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_register_xml_preload</name>
      <anchor>a3</anchor>
      <arglist>(const char *filename)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unregister_xml_preload</name>
      <anchor>a4</anchor>
      <arglist>(const char *filename)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_xml_init_defaults</name>
      <anchor>a5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_xml_parse_file</name>
      <anchor>a6</anchor>
      <arglist>(const char *file)</arglist>
    </member>
    <member kind="function">
      <type>long</type>
      <name>stp_xmlstrtol</name>
      <anchor>a7</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>unsigned long</type>
      <name>stp_xmlstrtoul</name>
      <anchor>a8</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_xmlstrtod</name>
      <anchor>a9</anchor>
      <arglist>(const char *textval)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_init</name>
      <anchor>a10</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_exit</name>
      <anchor>a11</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xml_get_node</name>
      <anchor>a12</anchor>
      <arglist>(stp_mxml_node_t *xmlroot,...)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmldoc_create_generic</name>
      <anchor>a13</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_preinit</name>
      <anchor>a14</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create_from_xmltree</name>
      <anchor>a15</anchor>
      <arglist>(stp_mxml_node_t *da)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmltree_create_from_sequence</name>
      <anchor>a16</anchor>
      <arglist>(const stp_sequence_t *seq)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_xmltree</name>
      <anchor>a17</anchor>
      <arglist>(stp_mxml_node_t *da)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmltree_create_from_curve</name>
      <anchor>a18</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create_from_xmltree</name>
      <anchor>a19</anchor>
      <arglist>(stp_mxml_node_t *array)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmltree_create_from_array</name>
      <anchor>a20</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_parse_file_named</name>
      <anchor>a21</anchor>
      <arglist>(const char *name)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>array.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>array_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stp_array</class>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_array</name>
      <anchor>a0</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>array_ctor</name>
      <anchor>a1</anchor>
      <arglist>(stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create</name>
      <anchor>ga2</anchor>
      <arglist>(int x_size, int y_size)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>array_dtor</name>
      <anchor>a3</anchor>
      <arglist>(stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_destroy</name>
      <anchor>ga4</anchor>
      <arglist>(stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_copy</name>
      <anchor>ga5</anchor>
      <arglist>(stp_array_t *dest, const stp_array_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create_copy</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_set_size</name>
      <anchor>ga7</anchor>
      <arglist>(stp_array_t *array, int x_size, int y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_get_size</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_array_t *array, int *x_size, int *y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_set_data</name>
      <anchor>ga9</anchor>
      <arglist>(stp_array_t *array, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_get_data</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_array_t *array, size_t *size, const double **data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_array_set_point</name>
      <anchor>ga11</anchor>
      <arglist>(stp_array_t *array, int x, int y, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_array_get_point</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_array_t *array, int x, int y, double *data)</arglist>
    </member>
    <member kind="function">
      <type>const stp_sequence_t *</type>
      <name>stp_array_get_sequence</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create_from_xmltree</name>
      <anchor>a14</anchor>
      <arglist>(stp_mxml_node_t *array)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmltree_create_from_array</name>
      <anchor>a15</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>bit-ops.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>bit-ops_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>SH20</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SH21</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SH40</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SH41</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SH42</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SH43</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_fold</name>
      <anchor>a6</anchor>
      <arglist>(const unsigned char *line, int single_length, unsigned char *outbuf)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_split_2_1</name>
      <anchor>a7</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *outhi, unsigned char *outlo)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stp_split_2_2</name>
      <anchor>a8</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *outhi, unsigned char *outlo)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_split_2</name>
      <anchor>a9</anchor>
      <arglist>(int length, int bits, const unsigned char *in, unsigned char *outhi, unsigned char *outlo)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_split_4_1</name>
      <anchor>a10</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_split_4_2</name>
      <anchor>a11</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_split_4</name>
      <anchor>a12</anchor>
      <arglist>(int length, int bits, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_unpack_2_1</name>
      <anchor>a13</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_unpack_2_2</name>
      <anchor>a14</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unpack_2</name>
      <anchor>a15</anchor>
      <arglist>(int length, int bits, const unsigned char *in, unsigned char *outlo, unsigned char *outhi)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_unpack_4_1</name>
      <anchor>a16</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_unpack_4_2</name>
      <anchor>a17</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unpack_4</name>
      <anchor>a18</anchor>
      <arglist>(int length, int bits, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_unpack_8_1</name>
      <anchor>a19</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3, unsigned char *out4, unsigned char *out5, unsigned char *out6, unsigned char *out7)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_unpack_8_2</name>
      <anchor>a20</anchor>
      <arglist>(int length, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3, unsigned char *out4, unsigned char *out5, unsigned char *out6, unsigned char *out7)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unpack_8</name>
      <anchor>a21</anchor>
      <arglist>(int length, int bits, const unsigned char *in, unsigned char *out0, unsigned char *out1, unsigned char *out2, unsigned char *out3, unsigned char *out4, unsigned char *out5, unsigned char *out6, unsigned char *out7)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>find_first_and_last</name>
      <anchor>a22</anchor>
      <arglist>(const unsigned char *line, int length, int *first, int *last)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_pack_uncompressed</name>
      <anchor>a23</anchor>
      <arglist>(stp_vars_t *v, const unsigned char *line, int length, unsigned char *comp_buf, unsigned char **comp_ptr, int *first, int *last)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_pack_tiff</name>
      <anchor>a24</anchor>
      <arglist>(stp_vars_t *v, const unsigned char *line, int length, unsigned char *comp_buf, unsigned char **comp_ptr, int *first, int *last)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>channel.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>channel_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stpi_subchannel_t</class>
    <class kind="struct">stpi_channel_t</class>
    <class kind="struct">stpi_channel_group_t</class>
    <member kind="function" static="yes">
      <type>void</type>
      <name>clear_a_channel</name>
      <anchor>a0</anchor>
      <arglist>(stpi_channel_group_t *cg, int channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_channel_clear</name>
      <anchor>a1</anchor>
      <arglist>(void *vc)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_reset</name>
      <anchor>a2</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_reset_channel</name>
      <anchor>a3</anchor>
      <arglist>(stp_vars_t *v, int channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_channel_free</name>
      <anchor>a4</anchor>
      <arglist>(void *vc)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stpi_subchannel_t *</type>
      <name>get_channel</name>
      <anchor>a5</anchor>
      <arglist>(stp_vars_t *v, unsigned channel, unsigned subchannel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_add</name>
      <anchor>a6</anchor>
      <arglist>(stp_vars_t *v, unsigned channel, unsigned subchannel, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_density_adjustment</name>
      <anchor>a7</anchor>
      <arglist>(stp_vars_t *v, int color, int subchannel, double adjustment)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_ink_limit</name>
      <anchor>a8</anchor>
      <arglist>(stp_vars_t *v, double limit)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_black_channel</name>
      <anchor>a9</anchor>
      <arglist>(stp_vars_t *v, int channel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_set_cutoff_adjustment</name>
      <anchor>a10</anchor>
      <arglist>(stp_vars_t *v, int color, int subchannel, double adjustment)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>input_needs_splitting</name>
      <anchor>a11</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_initialize</name>
      <anchor>a12</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int input_channel_count)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>clear_channel</name>
      <anchor>a13</anchor>
      <arglist>(unsigned short *data, unsigned width, unsigned depth)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>scale_channel</name>
      <anchor>a14</anchor>
      <arglist>(unsigned short *data, unsigned width, unsigned depth, unsigned short density)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>scan_channel</name>
      <anchor>a15</anchor>
      <arglist>(unsigned short *data, unsigned width, unsigned depth)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>ink_sum</name>
      <anchor>a16</anchor>
      <arglist>(const unsigned short *data, int total_channels)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>limit_ink</name>
      <anchor>a17</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mem_eq</name>
      <anchor>a18</anchor>
      <arglist>(const unsigned short *i1, const unsigned short *i2, int count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_channel_convert</name>
      <anchor>a19</anchor>
      <arglist>(const stp_vars_t *v, unsigned *zero_mask)</arglist>
    </member>
    <member kind="function">
      <type>unsigned short *</type>
      <name>stp_channel_get_input</name>
      <anchor>a20</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>unsigned short *</type>
      <name>stp_channel_get_output</name>
      <anchor>a21</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>color-conversion.h</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>color-conversion_8h</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="curve-cache_8h" name="curve-cache.h" local="no">gimp-print/curve-cache.h</includes>
    <class kind="struct">color_correction_t</class>
    <class kind="struct">channel_param_t</class>
    <class kind="struct">color_description_t</class>
    <class kind="struct">channel_depth_t</class>
    <class kind="struct">lut_t</class>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_K</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_C</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_M</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_Y</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_W</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_R</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_G</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_B</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_MAX</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_K</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_C</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_M</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_Y</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_W</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_R</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_G</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_B</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_RAW</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_NONE</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_RGB</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_CMY</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_CMYK</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_CMYKRB</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_ALL</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMASK_EVERY</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>unsigned(*</type>
      <name>stp_convert_t</name>
      <anchor>a25</anchor>
      <arglist>)(const stp_vars_t *vars, const unsigned char *in, unsigned short *out)</arglist>
    </member>
    <member kind="enumeration">
      <name>color_correction_enum_t</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_DEFAULT</name>
      <anchor>a51a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_UNCORRECTED</name>
      <anchor>a51a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_BRIGHT</name>
      <anchor>a51a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_ACCURATE</name>
      <anchor>a51a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_THRESHOLD</name>
      <anchor>a51a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_DESATURATED</name>
      <anchor>a51a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_DENSITY</name>
      <anchor>a51a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_RAW</name>
      <anchor>a51a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CORRECTION_PREDITHERED</name>
      <anchor>a51a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>color_model_t</name>
      <anchor>a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_WHITE</name>
      <anchor>a52a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_BLACK</name>
      <anchor>a52a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_UNKNOWN</name>
      <anchor>a52a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>color_id_t</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_GRAY</name>
      <anchor>a53a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_WHITE</name>
      <anchor>a53a39</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_RGB</name>
      <anchor>a53a40</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_CMY</name>
      <anchor>a53a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_CMYK</name>
      <anchor>a53a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_KCMY</name>
      <anchor>a53a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_CMYKRB</name>
      <anchor>a53a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_ID_RAW</name>
      <anchor>a53a45</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_gray</name>
      <anchor>a46</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *, unsigned short *)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_color</name>
      <anchor>a47</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *, unsigned short *)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_kcmy</name>
      <anchor>a48</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *, unsigned short *)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_cmykrb</name>
      <anchor>a49</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *, unsigned short *)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_raw</name>
      <anchor>a50</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *, unsigned short *)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>color-conversions.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>color-conversions_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="curve-cache_8h" name="curve-cache.h" local="no">gimp-print/curve-cache.h</includes>
    <includes id="color-conversion_8h" name="color-conversion.h" local="yes">color-conversion.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>LUM_RED</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LUM_GREEN</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LUM_BLUE</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FMAX</name>
      <anchor>a3</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FMIN</name>
      <anchor>a4</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GENERIC_COLOR_FUNC</name>
      <anchor>a5</anchor>
      <arglist>(fromname, toname)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_COLOR_FUNC</name>
      <anchor>a6</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FAST_COLOR_TO_COLOR_FUNC</name>
      <anchor>a7</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RAW_COLOR_TO_COLOR_FUNC</name>
      <anchor>a8</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GRAY_TO_COLOR_FUNC</name>
      <anchor>a9</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GRAY_TO_COLOR_RAW_FUNC</name>
      <anchor>a10</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_KCMY_FUNC</name>
      <anchor>a11</anchor>
      <arglist>(name, name2, name3, name4, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_KCMY_THRESHOLD_FUNC</name>
      <anchor>a12</anchor>
      <arglist>(T, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMYK_TO_KCMY_THRESHOLD_FUNC</name>
      <anchor>a13</anchor>
      <arglist>(T, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KCMY_TO_KCMY_THRESHOLD_FUNC</name>
      <anchor>a14</anchor>
      <arglist>(T, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GRAY_TO_COLOR_THRESHOLD_FUNC</name>
      <anchor>a15</anchor>
      <arglist>(T, name, bits, channels)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_COLOR_THRESHOLD_FUNC</name>
      <anchor>a16</anchor>
      <arglist>(T, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_GRAY_THRESHOLD_FUNC</name>
      <anchor>a17</anchor>
      <arglist>(T, name, channels, max_channels)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMYK_TO_COLOR_FUNC</name>
      <anchor>a18</anchor>
      <arglist>(namein, name2, T, bits, offset)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMYK_TO_KCMY_FUNC</name>
      <anchor>a19</anchor>
      <arglist>(T, size)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KCMY_TO_KCMY_FUNC</name>
      <anchor>a20</anchor>
      <arglist>(T, size)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GRAY_TO_GRAY_FUNC</name>
      <anchor>a21</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_GRAY_FUNC</name>
      <anchor>a22</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMYK_TO_GRAY_FUNC</name>
      <anchor>a23</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KCMY_TO_GRAY_FUNC</name>
      <anchor>a24</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GRAY_TO_GRAY_RAW_FUNC</name>
      <anchor>a25</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_GRAY_RAW_FUNC</name>
      <anchor>a26</anchor>
      <arglist>(T, bits, invertable, name2)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMYK_TO_GRAY_RAW_FUNC</name>
      <anchor>a27</anchor>
      <arglist>(T, bits, invertable, name2)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KCMY_TO_GRAY_RAW_FUNC</name>
      <anchor>a28</anchor>
      <arglist>(T, bits, invertable, name2)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMYK_TO_KCMY_RAW_FUNC</name>
      <anchor>a29</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>KCMY_TO_KCMY_RAW_FUNC</name>
      <anchor>a30</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_TO_CMYKRB_FUNC</name>
      <anchor>a31</anchor>
      <arglist>(name, name2, name3, name4, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DESATURATED_FUNC</name>
      <anchor>a32</anchor>
      <arglist>(name, name2, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CMYK_DISPATCH</name>
      <anchor>a33</anchor>
      <arglist>(name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RAW_TO_RAW_THRESHOLD_FUNC</name>
      <anchor>a34</anchor>
      <arglist>(T, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RAW_TO_RAW_FUNC</name>
      <anchor>a35</anchor>
      <arglist>(T, size)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RAW_TO_RAW_RAW_FUNC</name>
      <anchor>a36</anchor>
      <arglist>(T, bits)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONVERSION_FUNCTION_WITH_FAST</name>
      <anchor>a37</anchor>
      <arglist>(from, to, from2)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONVERSION_FUNCTION_WITHOUT_FAST</name>
      <anchor>a38</anchor>
      <arglist>(from, to, from2)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CONVERSION_FUNCTION_WITHOUT_DESATURATED</name>
      <anchor>a39</anchor>
      <arglist>(from, to, from2)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>calc_rgb_to_hsl</name>
      <anchor>a40</anchor>
      <arglist>(unsigned short *rgb, double *hue, double *sat, double *lightness)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>hsl_value</name>
      <anchor>a41</anchor>
      <arglist>(double n1, double n2, double hue)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>calc_hsl_to_rgb</name>
      <anchor>a42</anchor>
      <arglist>(unsigned short *rgb, double h, double s, double l)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>update_saturation</name>
      <anchor>a43</anchor>
      <arglist>(double sat, double adjust, double isat)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>interpolate_value</name>
      <anchor>a44</anchor>
      <arglist>(const double *vec, double val)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>update_saturation_from_rgb</name>
      <anchor>a45</anchor>
      <arglist>(unsigned short *rgb, const unsigned short *brightness_lookup, double adjust, double isat, int do_usermap)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>adjust_hue</name>
      <anchor>a46</anchor>
      <arglist>(const double *hue_map, double hue, size_t points)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>adjust_hsl</name>
      <anchor>a47</anchor>
      <arglist>(unsigned short *rgbout, lut_t *lut, double ssat, double isat, int split_saturation)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>adjust_hsl_bright</name>
      <anchor>a48</anchor>
      <arglist>(unsigned short *rgbout, lut_t *lut, double ssat, double isat, int split_saturation)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>lookup_rgb</name>
      <anchor>a49</anchor>
      <arglist>(lut_t *lut, unsigned short *rgbout, const unsigned short *red, const unsigned short *green, const unsigned short *blue, unsigned steps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>short_eq</name>
      <anchor>a50</anchor>
      <arglist>(const unsigned short *i1, const unsigned short *i2, size_t count)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>short_copy</name>
      <anchor>a51</anchor>
      <arglist>(unsigned short *out, const unsigned short *in, size_t count)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>generic_cmy_to_kcmy</name>
      <anchor>a52</anchor>
      <arglist>(const stp_vars_t *vars, const unsigned short *in, unsigned short *out)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>raw_cmy_to_kcmy</name>
      <anchor>a53</anchor>
      <arglist>(const stp_vars_t *vars, const unsigned short *in, unsigned short *out)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>generic_kcmy_to_cmykrb</name>
      <anchor>a54</anchor>
      <arglist>(const stp_vars_t *vars, const unsigned short *in, unsigned short *out)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>raw_kcmy_to_cmykrb</name>
      <anchor>a55</anchor>
      <arglist>(const stp_vars_t *vars, const unsigned short *in, unsigned short *out)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_gray</name>
      <anchor>a56</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *in, unsigned short *out)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_color</name>
      <anchor>a57</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *in, unsigned short *out)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_kcmy</name>
      <anchor>a58</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *in, unsigned short *out)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_to_cmykrb</name>
      <anchor>a59</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *in, unsigned short *out)</arglist>
    </member>
    <member kind="function">
      <type>unsigned</type>
      <name>stpi_color_convert_raw</name>
      <anchor>a60</anchor>
      <arglist>(const stp_vars_t *v, const unsigned char *in, unsigned short *out)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>color.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>color_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>stpi_color_namefunc</name>
      <anchor>a1</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>stpi_color_long_namefunc</name>
      <anchor>a2</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_init_color_list</name>
      <anchor>a3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_list</name>
      <anchor>a4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_count</name>
      <anchor>ga5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_color</name>
      <anchor>a6</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_index</name>
      <anchor>ga7</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_color_get_name</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_color_get_long_name</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const stp_colorfuncs_t *</type>
      <name>stpi_get_colorfuncs</name>
      <anchor>a10</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_name</name>
      <anchor>ga11</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_colorfuncs</name>
      <anchor>ga12</anchor>
      <arglist>(stp_colorfuncs_t *colorfuncs)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_init</name>
      <anchor>ga13</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, size_t steps)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_get_row</name>
      <anchor>ga14</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int row, unsigned *zero_mask)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_color_list_parameters</name>
      <anchor>ga15</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_color_describe_parameter</name>
      <anchor>ga16</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_register</name>
      <anchor>ga17</anchor>
      <arglist>(const stp_color_t *color)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_unregister</name>
      <anchor>ga18</anchor>
      <arglist>(const stp_color_t *color)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_list_t *</type>
      <name>color_list</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>curve-cache.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>curve-cache_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="curve-cache_8h" name="curve-cache.h" local="no">gimp-print/curve-cache.h</includes>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_free_curve_cache</name>
      <anchor>a0</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_curve_data</name>
      <anchor>a1</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_cache_get_curve</name>
      <anchor>a2</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_curve_invalidate</name>
      <anchor>a3</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_set_curve</name>
      <anchor>a4</anchor>
      <arglist>(stp_cached_curve_t *cache, stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_set_curve_copy</name>
      <anchor>a5</anchor>
      <arglist>(stp_cached_curve_t *cache, const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>const size_t</type>
      <name>stp_curve_cache_get_count</name>
      <anchor>a6</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned short *</type>
      <name>stp_curve_cache_get_ushort_data</name>
      <anchor>a7</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>const double *</type>
      <name>stp_curve_cache_get_double_data</name>
      <anchor>a8</anchor>
      <arglist>(stp_cached_curve_t *cache)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_cache_copy</name>
      <anchor>a9</anchor>
      <arglist>(stp_cached_curve_t *dest, const stp_cached_curve_t *src)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>curve.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>curve_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stp_curve</class>
    <member kind="define">
      <type>#define</type>
      <name>inline</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEFINE_DATA_SETTER</name>
      <anchor>a1</anchor>
      <arglist>(t, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEFINE_DATA_ACCESSOR</name>
      <anchor>a2</anchor>
      <arglist>(t, name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_curve</name>
      <anchor>a8</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>size_t</type>
      <name>get_real_point_count</name>
      <anchor>a9</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>size_t</type>
      <name>get_point_count</name>
      <anchor>a10</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>invalidate_auxiliary_data</name>
      <anchor>a11</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>clear_curve_data</name>
      <anchor>a12</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_linear_deltas</name>
      <anchor>a13</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_spline_deltas_piecewise</name>
      <anchor>a14</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_spline_deltas_dense</name>
      <anchor>a15</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_spline_deltas</name>
      <anchor>a16</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_intervals</name>
      <anchor>a17</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_curve_set_points</name>
      <anchor>a18</anchor>
      <arglist>(stp_curve_t *curve, size_t points)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_curve_ctor</name>
      <anchor>a19</anchor>
      <arglist>(stp_curve_t *curve, stp_curve_wrap_mode_t wrap_mode)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create</name>
      <anchor>ga20</anchor>
      <arglist>(stp_curve_wrap_mode_t wrap_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>curve_dtor</name>
      <anchor>a21</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_destroy</name>
      <anchor>ga22</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_copy</name>
      <anchor>ga23</anchor>
      <arglist>(stp_curve_t *dest, const stp_curve_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_copy</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_bounds</name>
      <anchor>ga25</anchor>
      <arglist>(stp_curve_t *curve, double low, double high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_get_bounds</name>
      <anchor>ga26</anchor>
      <arglist>(const stp_curve_t *curve, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_get_range</name>
      <anchor>ga27</anchor>
      <arglist>(const stp_curve_t *curve, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_curve_count_points</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_wrap_mode_t</type>
      <name>stp_curve_get_wrap</name>
      <anchor>ga29</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_is_piecewise</name>
      <anchor>ga30</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_interpolation_type</name>
      <anchor>ga31</anchor>
      <arglist>(stp_curve_t *curve, stp_curve_type_t itype)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_type_t</type>
      <name>stp_curve_get_interpolation_type</name>
      <anchor>ga32</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_gamma</name>
      <anchor>ga33</anchor>
      <arglist>(stp_curve_t *curve, double fgamma)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_curve_get_gamma</name>
      <anchor>ga34</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_data</name>
      <anchor>ga35</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_data_points</name>
      <anchor>ga36</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const stp_curve_point_t *data)</arglist>
    </member>
    <member kind="function">
      <type>const double *</type>
      <name>stp_curve_get_data</name>
      <anchor>ga37</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const stp_curve_point_t *</type>
      <name>stp_curve_get_data_points</name>
      <anchor>ga38</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const double *</type>
      <name>stpi_curve_get_data_internal</name>
      <anchor>a39</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_get_subrange</name>
      <anchor>ga40</anchor>
      <arglist>(const stp_curve_t *curve, size_t start, size_t count)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_subrange</name>
      <anchor>ga41</anchor>
      <arglist>(stp_curve_t *curve, const stp_curve_t *range, size_t start)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_point</name>
      <anchor>ga42</anchor>
      <arglist>(stp_curve_t *curve, size_t where, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_get_point</name>
      <anchor>ga43</anchor>
      <arglist>(const stp_curve_t *curve, size_t where, double *data)</arglist>
    </member>
    <member kind="function">
      <type>const stp_sequence_t *</type>
      <name>stp_curve_get_sequence</name>
      <anchor>ga44</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_rescale</name>
      <anchor>ga45</anchor>
      <arglist>(stp_curve_t *curve, double scale, stp_curve_compose_t mode, stp_curve_bounds_t bounds_mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_curve_check_parameters</name>
      <anchor>a46</anchor>
      <arglist>(stp_curve_t *curve, size_t points)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>interpolate_gamma_internal</name>
      <anchor>a47</anchor>
      <arglist>(const stp_curve_t *curve, double where)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>do_interpolate_spline</name>
      <anchor>a48</anchor>
      <arglist>(double low, double high, double frac, double interval_low, double interval_high, double x_interval)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>interpolate_point_internal</name>
      <anchor>a49</anchor>
      <arglist>(stp_curve_t *curve, double where)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_interpolate_value</name>
      <anchor>ga50</anchor>
      <arglist>(const stp_curve_t *curve, double where, double *result)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_resample</name>
      <anchor>ga51</anchor>
      <arglist>(stp_curve_t *curve, size_t points)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>gcd</name>
      <anchor>a52</anchor>
      <arglist>(unsigned a, unsigned b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>lcm</name>
      <anchor>a53</anchor>
      <arglist>(unsigned a, unsigned b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>create_gamma_curve</name>
      <anchor>a54</anchor>
      <arglist>(stp_curve_t **retval, double lo, double hi, double fgamma, int points)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>interpolate_points</name>
      <anchor>a55</anchor>
      <arglist>(stp_curve_t *a, stp_curve_t *b, stp_curve_compose_t mode, int points, double *tmp_data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_compose</name>
      <anchor>ga56</anchor>
      <arglist>(stp_curve_t **retval, stp_curve_t *a, stp_curve_t *b, stp_curve_compose_t mode, int points)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_xmltree</name>
      <anchor>a57</anchor>
      <arglist>(stp_mxml_node_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmltree_create_from_curve</name>
      <anchor>a58</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_mxml_node_t *</type>
      <name>xmldoc_create_from_curve</name>
      <anchor>a59</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>curve_whitespace_callback</name>
      <anchor>a60</anchor>
      <arglist>(stp_mxml_node_t *node, int where)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_write</name>
      <anchor>ga61</anchor>
      <arglist>(FILE *file, const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_curve_write_string</name>
      <anchor>ga62</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_curve_t *</type>
      <name>xml_doc_get_curve</name>
      <anchor>a63</anchor>
      <arglist>(stp_mxml_node_t *doc)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_file</name>
      <anchor>ga64</anchor>
      <arglist>(const char *file)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_stream</name>
      <anchor>ga65</anchor>
      <arglist>(FILE *fp)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_string</name>
      <anchor>ga66</anchor>
      <arglist>(const char *string)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>curve_point_limit</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char *const </type>
      <name>stpi_curve_type_names</name>
      <anchor>a4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>stpi_curve_type_count</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char *const </type>
      <name>stpi_wrap_mode_names</name>
      <anchor>a6</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>stpi_wrap_mode_count</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-ed.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-ed_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <includes id="dither-inlined-functions_8h" name="dither-inlined-functions.h" local="yes">dither-inlined-functions.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>UPDATE_COLOR</name>
      <anchor>a0</anchor>
      <arglist>(color, dither)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>update_dither</name>
      <anchor>a1</anchor>
      <arglist>(stpi_dither_t *d, int channel, int width, int direction, int *error0, int *error1)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_color</name>
      <anchor>a2</anchor>
      <arglist>(const stpi_dither_t *d, stpi_dither_channel_t *dc, int x, int y, unsigned char bit, int length, int dontprint, int stpi_dither_type, const unsigned char *mask)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>shared_ed_initializer</name>
      <anchor>a3</anchor>
      <arglist>(stpi_dither_t *d, int row, int duplicate_line, int zero_mask, int length, int direction, int ****error, int **ndither)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>shared_ed_deinitializer</name>
      <anchor>a4</anchor>
      <arglist>(stpi_dither_t *d, int ***error, int *ndither)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_ed</name>
      <anchor>a5</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *raw, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-eventone.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-eventone_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <includes id="dither-inlined-functions_8h" name="dither-inlined-functions.h" local="yes">dither-inlined-functions.h</includes>
    <class kind="struct">distance_t</class>
    <class kind="struct">eventone_t</class>
    <class kind="struct">shade_segment</class>
    <member kind="define">
      <type>#define</type>
      <name>EVEN_C1</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>EVEN_C2</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UNITONE_C1</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>UNITONE_C2</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>shade_segment</type>
      <name>shade_distance_t</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>free_eventone_data</name>
      <anchor>a5</anchor>
      <arglist>(stpi_dither_t *d)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>et_setup</name>
      <anchor>a6</anchor>
      <arglist>(stpi_dither_t *d)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>et_initializer</name>
      <anchor>a7</anchor>
      <arglist>(stpi_dither_t *d, int duplicate_line, int zero_mask)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>advance_eventone_pre</name>
      <anchor>a8</anchor>
      <arglist>(shade_distance_t *sp, eventone_t *et, int x)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>eventone_update</name>
      <anchor>a9</anchor>
      <arglist>(stpi_dither_channel_t *dc, eventone_t *et, int x, int direction)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>diffuse_error</name>
      <anchor>a10</anchor>
      <arglist>(stpi_dither_channel_t *dc, eventone_t *et, int x, int direction)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>eventone_adjust</name>
      <anchor>a11</anchor>
      <arglist>(stpi_dither_channel_t *dc, eventone_t *et, int dither_point, unsigned int desired)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>unitone_adjust</name>
      <anchor>a12</anchor>
      <arglist>(stpi_dither_channel_t *dc, eventone_t *et, int dither_point, unsigned int desired)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>find_segment</name>
      <anchor>a13</anchor>
      <arglist>(stpi_dither_channel_t *dc, unsigned inkval, stpi_ink_defn_t *lower, stpi_ink_defn_t *upper)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>find_segment_and_ditherpoint</name>
      <anchor>a14</anchor>
      <arglist>(stpi_dither_channel_t *dc, unsigned inkval, stpi_ink_defn_t *lower, stpi_ink_defn_t *upper)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_ink</name>
      <anchor>a15</anchor>
      <arglist>(stpi_dither_t *d, unsigned char *tptr, const stpi_ink_defn_t *ink, unsigned char bit, int length)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_et</name>
      <anchor>a16</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *raw, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_ut</name>
      <anchor>a17</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *raw, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-impl.h</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-impl_8h</filename>
    <class kind="struct">stpi_dither_algorithm_t</class>
    <class kind="struct">ink_defn</class>
    <class kind="struct">dither_segment</class>
    <class kind="struct">dither_channel</class>
    <class kind="struct">dither</class>
    <member kind="define">
      <type>#define</type>
      <name>D_FLOYD_HYBRID</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_ADAPTIVE_BASE</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_ADAPTIVE_HYBRID</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_ORDERED_BASE</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_ORDERED</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_FAST_BASE</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_FAST</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_VERY_FAST</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_EVENTONE</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_UNITONE</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_HYBRID_EVENTONE</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_HYBRID_UNITONE</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>D_PREDITHERED</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DITHER_FAST_STEPS</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ERROR_ROWS</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_SPREAD</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL</name>
      <anchor>a16</anchor>
      <arglist>(d, c)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHANNEL_COUNT</name>
      <anchor>a17</anchor>
      <arglist>(d)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>USMIN</name>
      <anchor>a18</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ADVANCE_UNIDIRECTIONAL</name>
      <anchor>a19</anchor>
      <arglist>(d, bit, input, width, xerror, xstep, xmod)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ADVANCE_REVERSE</name>
      <anchor>a20</anchor>
      <arglist>(d, bit, input, width, xerror, xstep, xmod)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ADVANCE_BIDIRECTIONAL</name>
      <anchor>a21</anchor>
      <arglist>(d, bit, in, dir, width, xer, xstep, xmod, err, S)</arglist>
    </member>
    <member kind="typedef">
      <type>void</type>
      <name>stpi_ditherfunc_t</name>
      <anchor>a22</anchor>
      <arglist>(stp_vars_t *, int, const unsigned short *, int, int, const unsigned char *)</arglist>
    </member>
    <member kind="typedef">
      <type>ink_defn</type>
      <name>stpi_ink_defn_t</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>dither_segment</type>
      <name>stpi_dither_segment_t</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>dither_channel</type>
      <name>stpi_dither_channel_t</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>dither</type>
      <name>stpi_dither_t</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_reverse_row_ends</name>
      <anchor>a33</anchor>
      <arglist>(stpi_dither_t *d)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stpi_dither_translate_channel</name>
      <anchor>a34</anchor>
      <arglist>(stp_vars_t *v, unsigned channel, unsigned subchannel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_channel_destroy</name>
      <anchor>a35</anchor>
      <arglist>(stpi_dither_channel_t *channel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_finalize</name>
      <anchor>a36</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int *</type>
      <name>stpi_dither_get_errline</name>
      <anchor>a37</anchor>
      <arglist>(stpi_dither_t *d, int row, int color)</arglist>
    </member>
    <member kind="variable">
      <type>stpi_ditherfunc_t</type>
      <name>stpi_dither_predithered</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ditherfunc_t</type>
      <name>stpi_dither_very_fast</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ditherfunc_t</type>
      <name>stpi_dither_ordered</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ditherfunc_t</type>
      <name>stpi_dither_ed</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ditherfunc_t</type>
      <name>stpi_dither_et</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ditherfunc_t</type>
      <name>stpi_dither_ut</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-inks.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-inks_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <member kind="function">
      <type>int</type>
      <name>stpi_dither_translate_channel</name>
      <anchor>a0</anchor>
      <arglist>(stp_vars_t *v, unsigned channel, unsigned subchannel)</arglist>
    </member>
    <member kind="function">
      <type>unsigned char *</type>
      <name>stp_dither_get_channel</name>
      <anchor>a1</anchor>
      <arglist>(stp_vars_t *v, unsigned channel, unsigned subchannel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>insert_channel</name>
      <anchor>a2</anchor>
      <arglist>(stp_vars_t *v, stpi_dither_t *d, int channel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_channel_destroy</name>
      <anchor>a3</anchor>
      <arglist>(stpi_dither_channel_t *channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>initialize_channel</name>
      <anchor>a4</anchor>
      <arglist>(stp_vars_t *v, int channel, int subchannel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>insert_subchannel</name>
      <anchor>a5</anchor>
      <arglist>(stp_vars_t *v, stpi_dither_t *d, int channel, int subchannel)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_finalize</name>
      <anchor>a6</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_add_channel</name>
      <anchor>a7</anchor>
      <arglist>(stp_vars_t *v, unsigned char *data, unsigned channel, unsigned subchannel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_dither_finalize_ranges</name>
      <anchor>a8</anchor>
      <arglist>(stp_vars_t *v, stpi_dither_channel_t *dc)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_dither_set_ranges</name>
      <anchor>a9</anchor>
      <arglist>(stp_vars_t *v, int color, const stp_shade_t *shade, double density, double darkness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_inks_simple</name>
      <anchor>a10</anchor>
      <arglist>(stp_vars_t *v, int color, int nlevels, const double *levels, double density, double darkness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_inks_full</name>
      <anchor>a11</anchor>
      <arglist>(stp_vars_t *v, int color, int nshades, const stp_shade_t *shades, double density, double darkness)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_inks</name>
      <anchor>a12</anchor>
      <arglist>(stp_vars_t *v, int color, double density, double darkness, int nshades, const double *svalues, int ndotsizes, const double *dvalues)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-inlined-functions.h</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-inlined-functions_8h</filename>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>ditherpoint</name>
      <anchor>a0</anchor>
      <arglist>(const stpi_dither_t *d, stp_dither_matrix_impl_t *mat, int x)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_row_ends</name>
      <anchor>a1</anchor>
      <arglist>(stpi_dither_channel_t *dc, int x)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-main.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-main_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <includes id="generic-options_8h" name="generic-options.h" local="yes">generic-options.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>RETURN_DITHERFUNC</name>
      <anchor>a0</anchor>
      <arglist>(func, v)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_dither_list_parameters</name>
      <anchor>a6</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_describe_parameter</name>
      <anchor>a7</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stpi_ditherfunc_t *</type>
      <name>stpi_set_dither_function</name>
      <anchor>a8</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_adaptive_limit</name>
      <anchor>a9</anchor>
      <arglist>(stp_vars_t *v, double limit)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_ink_spread</name>
      <anchor>a10</anchor>
      <arglist>(stp_vars_t *v, int spread)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_randomizer</name>
      <anchor>a11</anchor>
      <arglist>(stp_vars_t *v, int i, double val)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_dither_free</name>
      <anchor>a12</anchor>
      <arglist>(void *vd)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_init</name>
      <anchor>a13</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int out_width, int xdpi, int ydpi)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_reverse_row_ends</name>
      <anchor>a14</anchor>
      <arglist>(stpi_dither_t *d)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_dither_get_first_position</name>
      <anchor>a15</anchor>
      <arglist>(stp_vars_t *v, int color, int subchannel)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_dither_get_last_position</name>
      <anchor>a16</anchor>
      <arglist>(stp_vars_t *v, int color, int subchannel)</arglist>
    </member>
    <member kind="function">
      <type>int *</type>
      <name>stpi_dither_get_errline</name>
      <anchor>a17</anchor>
      <arglist>(stpi_dither_t *d, int row, int color)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_internal</name>
      <anchor>a18</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *input, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither</name>
      <anchor>a19</anchor>
      <arglist>(stp_vars_t *v, int row, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stpi_dither_algorithm_t</type>
      <name>dither_algos</name>
      <anchor>a1</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>num_dither_algos</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const unsigned</type>
      <name>sq2</name>
      <anchor>a3</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>dither_parameters</name>
      <anchor>a4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>dither_parameter_count</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-ordered.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-ordered_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <includes id="dither-inlined-functions_8h" name="dither-inlined-functions.h" local="yes">dither-inlined-functions.h</includes>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_color_ordered</name>
      <anchor>a0</anchor>
      <arglist>(const stpi_dither_t *d, stpi_dither_channel_t *dc, int val, int x, int y, unsigned char bit, int length)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_ordered</name>
      <anchor>a1</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *raw, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-predithered.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-predithered_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <includes id="dither-inlined-functions_8h" name="dither-inlined-functions.h" local="yes">dither-inlined-functions.h</includes>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_color_very_fast</name>
      <anchor>a0</anchor>
      <arglist>(const stpi_dither_t *d, stpi_dither_channel_t *dc, int val, int x, int y, unsigned char bit, unsigned bits, int length)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_predithered</name>
      <anchor>a1</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *raw, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>dither-very-fast.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>dither-very-fast_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <includes id="dither-inlined-functions_8h" name="dither-inlined-functions.h" local="yes">dither-inlined-functions.h</includes>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_color_very_fast</name>
      <anchor>a0</anchor>
      <arglist>(const stpi_dither_t *d, stpi_dither_channel_t *dc, int val, int x, int y, unsigned char bit, unsigned bits, int length)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_dither_very_fast</name>
      <anchor>a1</anchor>
      <arglist>(stp_vars_t *v, int row, const unsigned short *raw, int duplicate_line, int zero_mask, const unsigned char *mask)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>escp2-channels.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>escp2-channels_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="print-escp2_8h" name="print-escp2.h" local="yes">print-escp2.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a0</anchor>
      <arglist>(name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a1</anchor>
      <arglist>(name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_INKLIST</name>
      <anchor>a2</anchor>
      <arglist>(tname, name, inks, text, papers, adjustments, shades)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a3</anchor>
      <arglist>(name)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a201</anchor>
      <arglist>(standard_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a202</anchor>
      <arglist>(x80_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a203</anchor>
      <arglist>(c80_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a204</anchor>
      <arglist>(c64_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a205</anchor>
      <arglist>(standard_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a206</anchor>
      <arglist>(f360_standard_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a207</anchor>
      <arglist>(x80_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a208</anchor>
      <arglist>(c80_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a209</anchor>
      <arglist>(c64_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a210</anchor>
      <arglist>(standard_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a211</anchor>
      <arglist>(f360_standard_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a212</anchor>
      <arglist>(x80_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a213</anchor>
      <arglist>(c80_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a214</anchor>
      <arglist>(c64_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a215</anchor>
      <arglist>(standard_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a216</anchor>
      <arglist>(x80_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a217</anchor>
      <arglist>(c80_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a218</anchor>
      <arglist>(c64_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a219</anchor>
      <arglist>(f360_standard_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a220</anchor>
      <arglist>(standard_red)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a221</anchor>
      <arglist>(standard_blue)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a222</anchor>
      <arglist>(standard_gloss)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a223</anchor>
      <arglist>(standard_photo_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a224</anchor>
      <arglist>(photo_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a225</anchor>
      <arglist>(f360_photo_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a226</anchor>
      <arglist>(extended_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a227</anchor>
      <arglist>(photo_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a228</anchor>
      <arglist>(extended_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a229</anchor>
      <arglist>(photo_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a230</anchor>
      <arglist>(extended_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a231</anchor>
      <arglist>(photo_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a232</anchor>
      <arglist>(f360_photo_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a233</anchor>
      <arglist>(j_extended_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a234</anchor>
      <arglist>(photo2_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a235</anchor>
      <arglist>(f360_photo2_yellow)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a236</anchor>
      <arglist>(photo2_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a237</anchor>
      <arglist>(f360_photo2_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a238</anchor>
      <arglist>(quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a239</anchor>
      <arglist>(c80_quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a240</anchor>
      <arglist>(c64_quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a241</anchor>
      <arglist>(f360_photo_cyan)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK_CHANNEL</name>
      <anchor>a242</anchor>
      <arglist>(f360_photo_magenta)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a243</anchor>
      <arglist>(standard_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a244</anchor>
      <arglist>(standard_photo_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a245</anchor>
      <arglist>(standard_gloss_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a246</anchor>
      <arglist>(standard_photo_gloss_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a247</anchor>
      <arglist>(photo2_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a248</anchor>
      <arglist>(f360_photo2_black)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a249</anchor>
      <arglist>(quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a250</anchor>
      <arglist>(c80_quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a251</anchor>
      <arglist>(c64_quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a252</anchor>
      <arglist>(standard_cmy)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a253</anchor>
      <arglist>(x80_cmy)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a254</anchor>
      <arglist>(c80_cmy)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a255</anchor>
      <arglist>(c64_cmy)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a256</anchor>
      <arglist>(standard_gloss_cmy)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a257</anchor>
      <arglist>(photo_cmyk)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a258</anchor>
      <arglist>(gloss_cmyk)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a259</anchor>
      <arglist>(photo_gloss_cmyk)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a260</anchor>
      <arglist>(f360_cmyk)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a261</anchor>
      <arglist>(photo_composite)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a262</anchor>
      <arglist>(f360_photo_composite)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a263</anchor>
      <arglist>(photoj_composite)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a264</anchor>
      <arglist>(f360_photoj_composite)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a265</anchor>
      <arglist>(one_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a266</anchor>
      <arglist>(two_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a267</anchor>
      <arglist>(f360_two_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a268</anchor>
      <arglist>(standard_three_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a269</anchor>
      <arglist>(x80_three_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a270</anchor>
      <arglist>(c80_three_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a271</anchor>
      <arglist>(c64_three_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a272</anchor>
      <arglist>(five_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a273</anchor>
      <arglist>(f360_five_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a274</anchor>
      <arglist>(six_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a275</anchor>
      <arglist>(f360_six_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a276</anchor>
      <arglist>(j_seven_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a277</anchor>
      <arglist>(seven_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_CHANNEL_SET</name>
      <anchor>a278</anchor>
      <arglist>(f360_seven_color_extended)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a279</anchor>
      <arglist>(&quot;None&quot;, cmy, cmy, N_(&quot;EPSON Standard Inks&quot;), standard, standard, standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a280</anchor>
      <arglist>(&quot;None&quot;, standard, standard, N_(&quot;EPSON Standard Inks&quot;), standard, standard, standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a281</anchor>
      <arglist>(&quot;quadtone&quot;, quadtone, quadtone, N_(&quot;Quadtone&quot;), standard, standard, quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a282</anchor>
      <arglist>(&quot;None&quot;, c80, c80, N_(&quot;EPSON Standard Inks&quot;), durabrite, durabrite, standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a283</anchor>
      <arglist>(&quot;Quadtone&quot;, c80_quadtone, c80_quadtone, N_(&quot;Quadtone&quot;), standard, standard, quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a284</anchor>
      <arglist>(&quot;None&quot;, c64, c64, N_(&quot;EPSON Standard Inks&quot;), durabrite, durabrite, standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a285</anchor>
      <arglist>(&quot;Quadtone&quot;, c64_quadtone, c64_quadtone, N_(&quot;Quadtone&quot;), standard, standard, quadtone)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a286</anchor>
      <arglist>(&quot;None&quot;, x80, x80, N_(&quot;EPSON Standard Inks&quot;), standard, standard, standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a287</anchor>
      <arglist>(&quot;None&quot;, gen1, photo, N_(&quot;EPSON Standard Inks&quot;), standard, photo, photo_gen1)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a288</anchor>
      <arglist>(&quot;None&quot;, photo_gen2, photo, N_(&quot;EPSON Standard Inks&quot;), standard, photo2, photo_gen2)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a289</anchor>
      <arglist>(&quot;None&quot;, photo_gen3, photo, N_(&quot;EPSON Standard Inks&quot;), standard, photo3, photo_gen3)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a290</anchor>
      <arglist>(&quot;None&quot;, pigment, photo, N_(&quot;EPSON Standard Inks&quot;), ultrachrome, ultrachrome_photo, stp2000)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a291</anchor>
      <arglist>(&quot;None&quot;, f360_photo, f360_photo, N_(&quot;EPSON Standard Inks&quot;), standard, sp960, esp960)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a292</anchor>
      <arglist>(&quot;ultraphoto&quot;, f360_ultra_photo7, f360_photo7, N_(&quot;UltraChrome Photo Black&quot;), ultrachrome, ultrachrome_photo, ultrachrome_photo)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a293</anchor>
      <arglist>(&quot;ultramatte&quot;, f360_ultra_matte7, f360_photo7, N_(&quot;UltraChrome Matte Black&quot;), ultrachrome, ultrachrome_matte, ultrachrome_matte)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a294</anchor>
      <arglist>(&quot;ultraphoto&quot;, ultra_photo7, photo7, N_(&quot;UltraChrome Photo Black&quot;), ultrachrome, ultrachrome_photo, ultrachrome_photo)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a295</anchor>
      <arglist>(&quot;ultramatte&quot;, ultra_matte7, photo7, N_(&quot;UltraChrome Matte Black&quot;), ultrachrome, ultrachrome_matte, ultrachrome_matte)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a296</anchor>
      <arglist>(&quot;cmykrbmatte&quot;, cmykrb_matte, cmykrb_matte, N_(&quot;Matte Black&quot;), standard, standard, standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKLIST</name>
      <anchor>a297</anchor>
      <arglist>(&quot;cmykrbphoto&quot;, cmykrb_photo, cmykrb_photo, N_(&quot;Photo Black&quot;), standard, standard, standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a298</anchor>
      <arglist>(cmy)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a299</anchor>
      <arglist>(standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a300</anchor>
      <arglist>(c80)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a301</anchor>
      <arglist>(c64)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a302</anchor>
      <arglist>(x80)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a303</anchor>
      <arglist>(photo_gen1)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a304</anchor>
      <arglist>(photo_gen2)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a305</anchor>
      <arglist>(photo_gen3)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a306</anchor>
      <arglist>(photo_pigment)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a307</anchor>
      <arglist>(f360_photo)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a308</anchor>
      <arglist>(f360_ultrachrome)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INKGROUP</name>
      <anchor>a309</anchor>
      <arglist>(ultrachrome)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_black_subchannels</name>
      <anchor>a4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>x80_black_subchannels</name>
      <anchor>a5</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c80_black_subchannels</name>
      <anchor>a6</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c64_black_subchannels</name>
      <anchor>a7</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_cyan_subchannels</name>
      <anchor>a8</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_standard_cyan_subchannels</name>
      <anchor>a9</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>x80_cyan_subchannels</name>
      <anchor>a10</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c80_cyan_subchannels</name>
      <anchor>a11</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c64_cyan_subchannels</name>
      <anchor>a12</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_magenta_subchannels</name>
      <anchor>a13</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_standard_magenta_subchannels</name>
      <anchor>a14</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>x80_magenta_subchannels</name>
      <anchor>a15</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c80_magenta_subchannels</name>
      <anchor>a16</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c64_magenta_subchannels</name>
      <anchor>a17</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_yellow_subchannels</name>
      <anchor>a18</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>x80_yellow_subchannels</name>
      <anchor>a19</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c80_yellow_subchannels</name>
      <anchor>a20</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c64_yellow_subchannels</name>
      <anchor>a21</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_standard_yellow_subchannels</name>
      <anchor>a22</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_red_subchannels</name>
      <anchor>a23</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_blue_subchannels</name>
      <anchor>a24</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_gloss_subchannels</name>
      <anchor>a25</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>standard_photo_black_subchannels</name>
      <anchor>a26</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>photo_black_subchannels</name>
      <anchor>a27</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_photo_black_subchannels</name>
      <anchor>a28</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>extended_black_subchannels</name>
      <anchor>a29</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>photo_cyan_subchannels</name>
      <anchor>a30</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>extended_cyan_subchannels</name>
      <anchor>a31</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>photo_magenta_subchannels</name>
      <anchor>a32</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>extended_magenta_subchannels</name>
      <anchor>a33</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>photo_yellow_subchannels</name>
      <anchor>a34</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_photo_yellow_subchannels</name>
      <anchor>a35</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>j_extended_yellow_subchannels</name>
      <anchor>a36</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>photo2_yellow_subchannels</name>
      <anchor>a37</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_photo2_yellow_subchannels</name>
      <anchor>a38</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>photo2_black_subchannels</name>
      <anchor>a39</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_photo2_black_subchannels</name>
      <anchor>a40</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>quadtone_subchannels</name>
      <anchor>a41</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c80_quadtone_subchannels</name>
      <anchor>a42</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>c64_quadtone_subchannels</name>
      <anchor>a43</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_photo_cyan_subchannels</name>
      <anchor>a44</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const physical_subchannel_t</type>
      <name>f360_photo_magenta_subchannels</name>
      <anchor>a45</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_black_channels</name>
      <anchor>a46</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const escp2_inkname_t</type>
      <name>stpi_escp2_default_black_inkset</name>
      <anchor>a47</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_photo_black_channels</name>
      <anchor>a48</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const escp2_inkname_t</type>
      <name>stpi_escp2_default_photo_black_inkset</name>
      <anchor>a49</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_gloss_black_channels</name>
      <anchor>a50</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const escp2_inkname_t</type>
      <name>stpi_escp2_default_gloss_black_inkset</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_photo_gloss_black_channels</name>
      <anchor>a52</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const escp2_inkname_t</type>
      <name>stpi_escp2_default_photo_gloss_black_inkset</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo2_black_channels</name>
      <anchor>a54</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>two_color_grayscale_inkset</name>
      <anchor>a55</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_photo2_black_channels</name>
      <anchor>a56</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_two_color_grayscale_inkset</name>
      <anchor>a57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>quadtone_channels</name>
      <anchor>a58</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>generic_quadtone_inkset</name>
      <anchor>a59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c80_quadtone_channels</name>
      <anchor>a60</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c80_generic_quadtone_inkset</name>
      <anchor>a61</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c64_quadtone_channels</name>
      <anchor>a62</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c64_generic_quadtone_inkset</name>
      <anchor>a63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_cmy_channels</name>
      <anchor>a64</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>three_color_composite_inkset</name>
      <anchor>a65</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>x80_cmy_channels</name>
      <anchor>a66</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>x80_three_color_composite_inkset</name>
      <anchor>a67</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c80_cmy_channels</name>
      <anchor>a68</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c80_three_color_composite_inkset</name>
      <anchor>a69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c64_cmy_channels</name>
      <anchor>a70</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c64_three_color_composite_inkset</name>
      <anchor>a71</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_gloss_cmy_channels</name>
      <anchor>a72</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>three_color_composite_gloss_inkset</name>
      <anchor>a73</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_cmyk_channels</name>
      <anchor>a74</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_standard_inkset</name>
      <anchor>a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo_cmyk_channels</name>
      <anchor>a76</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_photo_inkset</name>
      <anchor>a77</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>gloss_cmyk_channels</name>
      <anchor>a78</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_gloss_inkset</name>
      <anchor>a79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo_gloss_cmyk_channels</name>
      <anchor>a80</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_photo_gloss_inkset</name>
      <anchor>a81</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>x80_cmyk_channels</name>
      <anchor>a82</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>x80_four_color_standard_inkset</name>
      <anchor>a83</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c80_cmyk_channels</name>
      <anchor>a84</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c80_four_color_standard_inkset</name>
      <anchor>a85</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c64_cmyk_channels</name>
      <anchor>a86</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c64_four_color_standard_inkset</name>
      <anchor>a87</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_cmyk_channels</name>
      <anchor>a88</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_four_color_standard_inkset</name>
      <anchor>a89</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo_composite_channels</name>
      <anchor>a90</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>five_color_photo_composite_inkset</name>
      <anchor>a91</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_photo_composite_channels</name>
      <anchor>a92</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_five_color_photo_composite_inkset</name>
      <anchor>a93</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo_channels</name>
      <anchor>a94</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>six_color_photo_inkset</name>
      <anchor>a95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_photo_channels</name>
      <anchor>a96</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_six_color_photo_inkset</name>
      <anchor>a97</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photoj_composite_channels</name>
      <anchor>a98</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>j_six_color_enhanced_composite_inkset</name>
      <anchor>a99</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_photoj_composite_channels</name>
      <anchor>a100</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_j_six_color_enhanced_composite_inkset</name>
      <anchor>a101</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo2_channels</name>
      <anchor>a102</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>seven_color_enhanced_inkset</name>
      <anchor>a103</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_photo2_channels</name>
      <anchor>a104</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_seven_color_enhanced_inkset</name>
      <anchor>a105</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photoj_channels</name>
      <anchor>a106</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>j_seven_color_enhanced_inkset</name>
      <anchor>a107</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_photoj_channels</name>
      <anchor>a108</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_j_seven_color_enhanced_inkset</name>
      <anchor>a109</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_cmykrb_channels</name>
      <anchor>a110</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>standard_cmykrb_inkset</name>
      <anchor>a111</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo_cmykrb_channels</name>
      <anchor>a112</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>photo_cmykrb_inkset</name>
      <anchor>a113</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>gloss_cmykrb_channels</name>
      <anchor>a114</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>gloss_cmykrb_inkset</name>
      <anchor>a115</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>photo_gloss_cmykrb_channels</name>
      <anchor>a116</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>photo_gloss_cmykrb_inkset</name>
      <anchor>a117</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>one_color_extended_channels</name>
      <anchor>a118</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>one_color_extended_inkset</name>
      <anchor>a119</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>one_color_photo_extended_inkset</name>
      <anchor>a120</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>one_color_extended_gloss_inkset</name>
      <anchor>a121</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>one_color_photo_extended_gloss_inkset</name>
      <anchor>a122</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>two_color_extended_channels</name>
      <anchor>a123</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>two_color_extended_inkset</name>
      <anchor>a124</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_two_color_extended_channels</name>
      <anchor>a125</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_two_color_extended_inkset</name>
      <anchor>a126</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>standard_three_color_extended_channels</name>
      <anchor>a127</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>three_color_extended_inkset</name>
      <anchor>a128</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>x80_three_color_extended_channels</name>
      <anchor>a129</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>x80_three_color_extended_inkset</name>
      <anchor>a130</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c80_three_color_extended_channels</name>
      <anchor>a131</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c80_three_color_extended_inkset</name>
      <anchor>a132</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>c64_three_color_extended_channels</name>
      <anchor>a133</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c64_three_color_extended_inkset</name>
      <anchor>a134</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_extended_inkset</name>
      <anchor>a135</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_photo_extended_inkset</name>
      <anchor>a136</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>x80_four_color_extended_inkset</name>
      <anchor>a137</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c80_four_color_extended_inkset</name>
      <anchor>a138</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>c64_four_color_extended_inkset</name>
      <anchor>a139</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_four_color_extended_inkset</name>
      <anchor>a140</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_gloss_extended_inkset</name>
      <anchor>a141</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>four_color_photo_gloss_extended_inkset</name>
      <anchor>a142</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>five_color_extended_channels</name>
      <anchor>a143</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>five_color_extended_inkset</name>
      <anchor>a144</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_five_color_extended_channels</name>
      <anchor>a145</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_five_color_extended_inkset</name>
      <anchor>a146</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>six_color_extended_channels</name>
      <anchor>a147</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>six_color_extended_inkset</name>
      <anchor>a148</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>six_color_cmykrb_extended_inkset</name>
      <anchor>a149</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>six_color_cmykrb_photo_extended_inkset</name>
      <anchor>a150</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_six_color_extended_channels</name>
      <anchor>a151</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_six_color_extended_inkset</name>
      <anchor>a152</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>six_color_cmykrb_gloss_extended_inkset</name>
      <anchor>a153</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>six_color_cmykrb_photo_gloss_extended_inkset</name>
      <anchor>a154</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>j_seven_color_extended_channels</name>
      <anchor>a155</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>j_seven_color_extended_inkset</name>
      <anchor>a156</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>seven_color_extended_channels</name>
      <anchor>a157</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>seven_color_extended_inkset</name>
      <anchor>a158</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>f360_seven_color_extended_channels</name>
      <anchor>a159</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>f360_seven_color_extended_inkset</name>
      <anchor>a160</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_channel_t *const </type>
      <name>gloss_cmykprb_extended_channels</name>
      <anchor>a161</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t</type>
      <name>seven_color_cmykprb_gloss_extended_inkset</name>
      <anchor>a162</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>standard_shades</name>
      <anchor>a163</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>photo_gen1_shades</name>
      <anchor>a164</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>photo_gen2_shades</name>
      <anchor>a165</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>photo_gen3_shades</name>
      <anchor>a166</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>esp960_shades</name>
      <anchor>a167</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>stp2000_shades</name>
      <anchor>a168</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>ultrachrome_photo_shades</name>
      <anchor>a169</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>ultrachrome_matte_shades</name>
      <anchor>a170</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const shade_set_t</type>
      <name>quadtone_shades</name>
      <anchor>a171</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>cmy_ink_types</name>
      <anchor>a172</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>standard_ink_types</name>
      <anchor>a173</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>quadtone_ink_types</name>
      <anchor>a174</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>c80_ink_types</name>
      <anchor>a175</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>c80_quadtone_ink_types</name>
      <anchor>a176</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>c64_ink_types</name>
      <anchor>a177</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>c64_quadtone_ink_types</name>
      <anchor>a178</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>x80_ink_types</name>
      <anchor>a179</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>photo_ink_types</name>
      <anchor>a180</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>f360_photo_ink_types</name>
      <anchor>a181</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>f360_photo7_japan_ink_types</name>
      <anchor>a182</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>f360_photo7_ink_types</name>
      <anchor>a183</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>photo7_ink_types</name>
      <anchor>a184</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>cmykrb_matte_ink_types</name>
      <anchor>a185</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_inkname_t *const </type>
      <name>cmykrb_photo_ink_types</name>
      <anchor>a186</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>cmy_group</name>
      <anchor>a187</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>standard_group</name>
      <anchor>a188</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>c80_group</name>
      <anchor>a189</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>c64_group</name>
      <anchor>a190</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>x80_group</name>
      <anchor>a191</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>photo_gen1_group</name>
      <anchor>a192</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>photo_gen2_group</name>
      <anchor>a193</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>photo_gen3_group</name>
      <anchor>a194</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>photo_pigment_group</name>
      <anchor>a195</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>f360_photo_group</name>
      <anchor>a196</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>f360_photo7_japan_group</name>
      <anchor>a197</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>f360_ultrachrome_group</name>
      <anchor>a198</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>ultrachrome_group</name>
      <anchor>a199</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const inklist_t *const </type>
      <name>cmykrb_group</name>
      <anchor>a200</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>escp2-driver.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>escp2-driver_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="print-escp2_8h" name="print-escp2.h" local="yes">print-escp2.h</includes>
    <member kind="function" static="yes">
      <type>escp2_privdata_t *</type>
      <name>get_privdata</name>
      <anchor>a0</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_reset_printer</name>
      <anchor>a1</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_remote_param</name>
      <anchor>a2</anchor>
      <arglist>(stp_vars_t *v, const char *param, const char *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_remote_int_param</name>
      <anchor>a3</anchor>
      <arglist>(stp_vars_t *v, const char *param, int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_remote_float_param</name>
      <anchor>a4</anchor>
      <arglist>(stp_vars_t *v, const char *param, double value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>print_debug_params</name>
      <anchor>a5</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_remote_sequence</name>
      <anchor>a6</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_graphics_mode</name>
      <anchor>a7</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_resolution</name>
      <anchor>a8</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_color</name>
      <anchor>a9</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_printer_weave</name>
      <anchor>a10</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_printhead_speed</name>
      <anchor>a11</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_dot_size</name>
      <anchor>a12</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_page_height</name>
      <anchor>a13</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_margins</name>
      <anchor>a14</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_form_factor</name>
      <anchor>a15</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_set_printhead_resolution</name>
      <anchor>a16</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_vertical_position</name>
      <anchor>a17</anchor>
      <arglist>(stp_vars_t *v, stp_pass_t *pass)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_color</name>
      <anchor>a18</anchor>
      <arglist>(stp_vars_t *v, stp_pass_t *pass, int color)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_horizontal_position</name>
      <anchor>a19</anchor>
      <arglist>(stp_vars_t *v, stp_pass_t *pass, int vertical_subpass)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>send_print_command</name>
      <anchor>a20</anchor>
      <arglist>(stp_vars_t *v, stp_pass_t *pass, int color, int nlines)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>send_extra_data</name>
      <anchor>a21</anchor>
      <arglist>(stp_vars_t *v, int extralines)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_init_printer</name>
      <anchor>a22</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_deinit_printer</name>
      <anchor>a23</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_flush_pass</name>
      <anchor>a24</anchor>
      <arglist>(stp_vars_t *v, int passno, int vertical_subpass)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_terminate_page</name>
      <anchor>a25</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>escp2-inks.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>escp2-inks_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="print-escp2_8h" name="print-escp2.h" local="yes">print-escp2.h</includes>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_single_dropsizes</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_low_dropsizes</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_6pl_dropsizes</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_6pl_1440_dropsizes</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_6pl_2880_dropsizes</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_x80_low_dropsizes</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_x80_6pl_dropsizes</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_x80_1440_6pl_dropsizes</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_x80_2880_6pl_dropsizes</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_new_low_dropsizes</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_new_6pl_dropsizes</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_new_4pl_dropsizes</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_4pl_2880_dropsizes</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_4pl_dropsizes</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_3pl_dropsizes</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_3pl_1440_dropsizes</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_3pl_2880_dropsizes</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_980_6pl_dropsizes</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_2pl_360_dropsizes</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_2pl_720_dropsizes</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_2pl_1440_dropsizes</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_2pl_2880_dropsizes</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_1_5pl_360_dropsizes</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_1_5pl_720_dropsizes</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_1_5pl_1440_dropsizes</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_1_5pl_2880_dropsizes</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_r300_360_dropsizes</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_r300_720_dropsizes</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_r300_1440_dropsizes</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_r300_2880_dropsizes</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_r300_2880_1440_dropsizes</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_economy_pigment_dropsizes</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_low_pigment_dropsizes</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_6pl_pigment_dropsizes</name>
      <anchor>a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_3pl_pigment_dropsizes</name>
      <anchor>a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_3pl_pigment_2880_dropsizes</name>
      <anchor>a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_3pl_pigment_5760_dropsizes</name>
      <anchor>a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_2000p_dropsizes</name>
      <anchor>a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_ultrachrome_low_dropsizes</name>
      <anchor>a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_ultrachrome_720_dropsizes</name>
      <anchor>a39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_ultrachrome_2880_dropsizes</name>
      <anchor>a40</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dropsize_t</type>
      <name>escp2_spro10000_dropsizes</name>
      <anchor>a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_simple_drops</name>
      <anchor>a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_6pl_drops</name>
      <anchor>a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_x80_6pl_drops</name>
      <anchor>a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_1440_4pl_drops</name>
      <anchor>a45</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_2880_4pl_drops</name>
      <anchor>a46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_3pl_drops</name>
      <anchor>a47</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_2pl_drops</name>
      <anchor>a48</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_3pl_pmg_drops</name>
      <anchor>a49</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_1_5pl_drops</name>
      <anchor>a50</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_2000p_drops</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_ultrachrome_drops</name>
      <anchor>a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_3pl_pigment_drops</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_spro10000_drops</name>
      <anchor>a54</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>escp2-papers.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>escp2-papers_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="print-escp2_8h" name="print-escp2.h" local="yes">print-escp2.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_PAPERS</name>
      <anchor>a0</anchor>
      <arglist>(name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_PAPER_ADJUSTMENTS</name>
      <anchor>a1</anchor>
      <arglist>(name)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPER_ADJUSTMENTS</name>
      <anchor>a31</anchor>
      <arglist>(standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPER_ADJUSTMENTS</name>
      <anchor>a32</anchor>
      <arglist>(photo)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPER_ADJUSTMENTS</name>
      <anchor>a33</anchor>
      <arglist>(sp960)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPER_ADJUSTMENTS</name>
      <anchor>a34</anchor>
      <arglist>(ultrachrome_photo)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPER_ADJUSTMENTS</name>
      <anchor>a35</anchor>
      <arglist>(ultrachrome_matte)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPER_ADJUSTMENTS</name>
      <anchor>a36</anchor>
      <arglist>(durabrite)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPERS</name>
      <anchor>a37</anchor>
      <arglist>(standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPERS</name>
      <anchor>a38</anchor>
      <arglist>(durabrite)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PAPERS</name>
      <anchor>a39</anchor>
      <arglist>(ultrachrome)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_sat_adj</name>
      <anchor>a2</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_lum_adj</name>
      <anchor>a3</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_hue_adj</name>
      <anchor>a4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>photo2_sat_adj</name>
      <anchor>a5</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>photo2_lum_adj</name>
      <anchor>a6</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>photo2_hue_adj</name>
      <anchor>a7</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>sp960_sat_adj</name>
      <anchor>a8</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>sp960_lum_adj</name>
      <anchor>a9</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>sp960_hue_adj</name>
      <anchor>a10</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>sp960_matte_sat_adj</name>
      <anchor>a11</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>sp960_matte_lum_adj</name>
      <anchor>a12</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>sp960_matte_hue_adj</name>
      <anchor>a13</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>ultra_matte_sat_adj</name>
      <anchor>a14</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>ultra_matte_lum_adj</name>
      <anchor>a15</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>ultra_matte_hue_adj</name>
      <anchor>a16</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>ultra_glossy_sat_adj</name>
      <anchor>a17</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>ultra_glossy_lum_adj</name>
      <anchor>a18</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>ultra_glossy_hue_adj</name>
      <anchor>a19</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>standard_adjustments</name>
      <anchor>a20</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>photo_adjustments</name>
      <anchor>a21</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>photo2_adjustments</name>
      <anchor>a22</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>photo3_adjustments</name>
      <anchor>a23</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>sp960_adjustments</name>
      <anchor>a24</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>ultrachrome_photo_adjustments</name>
      <anchor>a25</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>ultrachrome_matte_adjustments</name>
      <anchor>a26</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_adjustment_t</type>
      <name>durabrite_adjustments</name>
      <anchor>a27</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_t</type>
      <name>standard_papers</name>
      <anchor>a28</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_t</type>
      <name>durabrite_papers</name>
      <anchor>a29</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_t</type>
      <name>ultrachrome_papers</name>
      <anchor>a30</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>escp2-resolutions.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>escp2-resolutions_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="print-escp2_8h" name="print-escp2.h" local="yes">print-escp2.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_PRINTER_WEAVES</name>
      <anchor>a0</anchor>
      <arglist>(name)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PRINTER_WEAVES</name>
      <anchor>a49</anchor>
      <arglist>(standard)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PRINTER_WEAVES</name>
      <anchor>a50</anchor>
      <arglist>(pro7000)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PRINTER_WEAVES</name>
      <anchor>a51</anchor>
      <arglist>(pro7500)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_PRINTER_WEAVES</name>
      <anchor>a52</anchor>
      <arglist>(pro7600)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x90dpi</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x90sw</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x120dpi</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x120sw</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_180dpi</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_180sw</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x180dpi</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x180sw</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x240dpi</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360x240sw</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360mw</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360pro</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_360sw</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_720x360mw</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_720x360sw</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_720mw</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_720sw</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_720hq</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_720hq2</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_1440x720mw</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_1440x720sw</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_1440x720hq2</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_2880x720mw</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_2880x720sw</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_2880x720hq2</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_1440x1440mw</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_1440x1440sw</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_1440x1440ov</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_2880x1440mw</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_2880x1440sw</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_2880x2880mw</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const res_t</type>
      <name>r_2880x2880sw</name>
      <anchor>a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_720dpi_reslist</name>
      <anchor>a34</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_1440dpi_reslist</name>
      <anchor>a35</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_2880dpi_reslist</name>
      <anchor>a36</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_2880_1440dpi_reslist</name>
      <anchor>a37</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_g3_reslist</name>
      <anchor>a38</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_superfine_reslist</name>
      <anchor>a39</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_sc500_reslist</name>
      <anchor>a40</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_g3_720dpi_reslist</name>
      <anchor>a41</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_720dpi_soft_reslist</name>
      <anchor>a42</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_sc640_reslist</name>
      <anchor>a43</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_pro_reslist</name>
      <anchor>a44</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const printer_weave_t</type>
      <name>standard_printer_weaves</name>
      <anchor>a45</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const printer_weave_t</type>
      <name>pro7000_printer_weaves</name>
      <anchor>a46</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const printer_weave_t</type>
      <name>pro7500_printer_weaves</name>
      <anchor>a47</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const printer_weave_t</type>
      <name>pro7600_printer_weaves</name>
      <anchor>a48</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>generic-options.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>generic-options_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="generic-options_8h" name="generic-options.h" local="yes">generic-options.h</includes>
    <member kind="function">
      <type>int</type>
      <name>stpi_get_qualities_count</name>
      <anchor>a5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_quality_t *</type>
      <name>stpi_get_quality_by_index</name>
      <anchor>a6</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_quality_t *</type>
      <name>stpi_get_quality_by_name</name>
      <anchor>a7</anchor>
      <arglist>(const char *quality)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stpi_get_image_types_count</name>
      <anchor>a8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_image_type_t *</type>
      <name>stpi_get_image_type_by_index</name>
      <anchor>a9</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_image_type_t *</type>
      <name>stpi_get_image_type_by_name</name>
      <anchor>a10</anchor>
      <arglist>(const char *image_type)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stpi_get_job_modes_count</name>
      <anchor>a11</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_job_mode_t *</type>
      <name>stpi_get_job_mode_by_index</name>
      <anchor>a12</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_job_mode_t *</type>
      <name>stpi_get_job_mode_by_name</name>
      <anchor>a13</anchor>
      <arglist>(const char *job_mode)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_list_generic_parameters</name>
      <anchor>a14</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_describe_generic_parameter</name>
      <anchor>a15</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stpi_quality_t</type>
      <name>standard_qualities</name>
      <anchor>a0</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stpi_image_type_t</type>
      <name>standard_image_types</name>
      <anchor>a1</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stpi_job_mode_t</type>
      <name>standard_job_modes</name>
      <anchor>a2</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a3</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>the_parameter_count</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>generic-options.h</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>generic-options_8h</filename>
    <class kind="struct">stpi_quality_t</class>
    <class kind="struct">stpi_image_type_t</class>
    <class kind="struct">stpi_job_mode_t</class>
    <member kind="function">
      <type>int</type>
      <name>stpi_get_qualities_count</name>
      <anchor>a0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_quality_t *</type>
      <name>stpi_get_quality_by_index</name>
      <anchor>a1</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_quality_t *</type>
      <name>stpi_get_quality_by_name</name>
      <anchor>a2</anchor>
      <arglist>(const char *quality)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stpi_get_image_types_count</name>
      <anchor>a3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_image_type_t *</type>
      <name>stpi_get_image_type_by_index</name>
      <anchor>a4</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_image_type_t *</type>
      <name>stpi_get_image_type_by_name</name>
      <anchor>a5</anchor>
      <arglist>(const char *image_type)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stpi_get_job_modes_count</name>
      <anchor>a6</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_job_mode_t *</type>
      <name>stpi_get_job_mode_by_index</name>
      <anchor>a7</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stpi_job_mode_t *</type>
      <name>stpi_get_job_mode_by_name</name>
      <anchor>a8</anchor>
      <arglist>(const char *job_mode)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_list_generic_parameters</name>
      <anchor>a9</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_describe_generic_parameter</name>
      <anchor>a10</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>gimp-print-internal.h</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>gimp-print-internal_8h</filename>
    <includes id="gimp-print-module_8h" name="gimp-print-module.h" local="no">gimp-print/gimp-print-module.h</includes>
    <includes id="src_2main_2util_8h" name="util.h" local="yes">util.h</includes>
  </compound>
  <compound kind="file">
    <name>image.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>image_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <member kind="function">
      <type>void</type>
      <name>stp_image_init</name>
      <anchor>ga0</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_reset</name>
      <anchor>ga1</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_image_width</name>
      <anchor>ga2</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_image_height</name>
      <anchor>ga3</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>stp_image_status_t</type>
      <name>stp_image_get_row</name>
      <anchor>ga4</anchor>
      <arglist>(stp_image_t *image, unsigned char *data, size_t byte_limit, int row)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_image_get_appname</name>
      <anchor>ga5</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_conclude</name>
      <anchor>ga6</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>module.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>module_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stpi_internal_module_class</class>
    <member kind="typedef">
      <type>stpi_internal_module_class</type>
      <name>stpi_internal_module_class_t</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>module_list_freefunc</name>
      <anchor>a12</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stp_module_register</name>
      <anchor>a13</anchor>
      <arglist>(stp_module_t *module)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_load</name>
      <anchor>a14</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_exit</name>
      <anchor>a15</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_module_get_class</name>
      <anchor>a16</anchor>
      <arglist>(stp_module_class_t class)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_open</name>
      <anchor>a17</anchor>
      <arglist>(const char *modulename)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_init</name>
      <anchor>a18</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_module_close</name>
      <anchor>a19</anchor>
      <arglist>(stp_list_item_t *module)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stpi_internal_module_class_t</type>
      <name>module_classes</name>
      <anchor>a1</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>print_canon_LTX_stp_module_data</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>print_escp2_LTX_stp_module_data</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>print_lexmark_LTX_stp_module_data</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>print_pcl_LTX_stp_module_data</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>print_ps_LTX_stp_module_data</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>print_olympus_LTX_stp_module_data</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>print_raw_LTX_stp_module_data</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>color_traditional_LTX_stp_module_data</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_module_t *</type>
      <name>static_modules</name>
      <anchor>a10</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_list_t *</type>
      <name>module_list</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>mxml-attr.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>mxml-attr_8c</filename>
    <includes id="mxml_8h" name="mxml.h" local="no">gimp-print/mxml.h</includes>
    <member kind="function">
      <type>const char *</type>
      <name>stp_mxmlElementGetAttr</name>
      <anchor>a0</anchor>
      <arglist>(stp_mxml_node_t *node, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlElementSetAttr</name>
      <anchor>a1</anchor>
      <arglist>(stp_mxml_node_t *node, const char *name, const char *value)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>mxml-file.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>mxml-file_8c</filename>
    <includes id="mxml_8h" name="mxml.h" local="no">gimp-print/mxml.h</includes>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_add_char</name>
      <anchor>a0</anchor>
      <arglist>(int ch, char **ptr, char **buffer, int *bufsize)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_file_getc</name>
      <anchor>a1</anchor>
      <arglist>(void *p)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_file_putc</name>
      <anchor>a2</anchor>
      <arglist>(int ch, void *p)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_mxml_node_t *</type>
      <name>mxml_load_data</name>
      <anchor>a3</anchor>
      <arglist>(stp_mxml_node_t *top, void *p, stp_mxml_type_t(*cb)(stp_mxml_node_t *), int(*getc_cb)(void *))</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_parse_element</name>
      <anchor>a4</anchor>
      <arglist>(stp_mxml_node_t *node, void *p, int(*getc_cb)(void *))</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_string_getc</name>
      <anchor>a5</anchor>
      <arglist>(void *p)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_string_putc</name>
      <anchor>a6</anchor>
      <arglist>(int ch, void *p)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_write_node</name>
      <anchor>a7</anchor>
      <arglist>(stp_mxml_node_t *node, void *p, int(*cb)(stp_mxml_node_t *, int), int col, int(*putc_cb)(int, void *))</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_write_string</name>
      <anchor>a8</anchor>
      <arglist>(const char *s, void *p, int(*putc_cb)(int, void *))</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>mxml_write_ws</name>
      <anchor>a9</anchor>
      <arglist>(stp_mxml_node_t *node, void *p, int(*cb)(stp_mxml_node_t *, int), int ws, int col, int(*putc_cb)(int, void *))</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlLoadFile</name>
      <anchor>a10</anchor>
      <arglist>(stp_mxml_node_t *top, FILE *fp, stp_mxml_type_t(*cb)(stp_mxml_node_t *))</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlLoadString</name>
      <anchor>a11</anchor>
      <arglist>(stp_mxml_node_t *top, const char *s, stp_mxml_type_t(*cb)(stp_mxml_node_t *))</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_mxmlSaveAllocString</name>
      <anchor>a12</anchor>
      <arglist>(stp_mxml_node_t *node, int(*cb)(stp_mxml_node_t *, int))</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_mxmlSaveFile</name>
      <anchor>a13</anchor>
      <arglist>(stp_mxml_node_t *node, FILE *fp, int(*cb)(stp_mxml_node_t *, int))</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_mxmlSaveString</name>
      <anchor>a14</anchor>
      <arglist>(stp_mxml_node_t *node, char *buffer, int bufsize, int(*cb)(stp_mxml_node_t *, int))</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>mxml-node.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>mxml-node_8c</filename>
    <includes id="mxml_8h" name="mxml.h" local="no">gimp-print/mxml.h</includes>
    <member kind="function" static="yes">
      <type>stp_mxml_node_t *</type>
      <name>mxml_new</name>
      <anchor>a0</anchor>
      <arglist>(stp_mxml_node_t *parent, stp_mxml_type_t type)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlAdd</name>
      <anchor>a1</anchor>
      <arglist>(stp_mxml_node_t *parent, int where, stp_mxml_node_t *child, stp_mxml_node_t *node)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlDelete</name>
      <anchor>a2</anchor>
      <arglist>(stp_mxml_node_t *node)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewElement</name>
      <anchor>a3</anchor>
      <arglist>(stp_mxml_node_t *parent, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewInteger</name>
      <anchor>a4</anchor>
      <arglist>(stp_mxml_node_t *parent, int integer)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewOpaque</name>
      <anchor>a5</anchor>
      <arglist>(stp_mxml_node_t *parent, const char *opaque)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewReal</name>
      <anchor>a6</anchor>
      <arglist>(stp_mxml_node_t *parent, double real)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlNewText</name>
      <anchor>a7</anchor>
      <arglist>(stp_mxml_node_t *parent, int whitespace, const char *string)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_mxmlRemove</name>
      <anchor>a8</anchor>
      <arglist>(stp_mxml_node_t *node)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>mxml-search.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>mxml-search_8c</filename>
    <includes id="mxml_8h" name="mxml.h" local="no">gimp-print/mxml.h</includes>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlFindElement</name>
      <anchor>a0</anchor>
      <arglist>(stp_mxml_node_t *node, stp_mxml_node_t *top, const char *name, const char *attr, const char *value, int descend)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlWalkNext</name>
      <anchor>a1</anchor>
      <arglist>(stp_mxml_node_t *node, stp_mxml_node_t *top, int descend)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_mxmlWalkPrev</name>
      <anchor>a2</anchor>
      <arglist>(stp_mxml_node_t *node, stp_mxml_node_t *top, int descend)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>path.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>path_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>_D_EXACT_NAMLEN</name>
      <anchor>a0</anchor>
      <arglist>(d)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>_D_ALLOC_NAMLEN</name>
      <anchor>a1</anchor>
      <arglist>(d)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_path_check</name>
      <anchor>a4</anchor>
      <arglist>(const struct dirent *module)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>char *</type>
      <name>stpi_path_merge</name>
      <anchor>a5</anchor>
      <arglist>(const char *path, const char *file)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_scandir</name>
      <anchor>a6</anchor>
      <arglist>(const char *dir, struct dirent ***namelist, int(*sel)(const struct dirent *), int(*cmp)(const void *, const void *))</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>dirent_sort</name>
      <anchor>a7</anchor>
      <arglist>(const void *a, const void *b)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_path_search</name>
      <anchor>a8</anchor>
      <arglist>(stp_list_t *dirlist, const char *suffix)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_path_split</name>
      <anchor>a9</anchor>
      <arglist>(stp_list_t *list, const char *path)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char *</type>
      <name>path_check_path</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char *</type>
      <name>path_check_suffix</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-canon.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-canon_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">canon_dot_sizes</class>
    <class kind="struct">canon_densities</class>
    <class kind="struct">canon_variable_ink</class>
    <class kind="struct">canon_variable_inkset</class>
    <class kind="struct">canon_variable_inklist</class>
    <class kind="struct">canon_caps</class>
    <class kind="struct">canon_privdata_t</class>
    <class kind="struct">canon_res_t</class>
    <class kind="struct">paper_t</class>
    <class kind="struct">canon_init_t</class>
    <class kind="struct">float_param_t</class>
    <member kind="define">
      <type>#define</type>
      <name>CHAR_BIT</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_CARRIAGE_WIDTH</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_PHYSICAL_BPI</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_OVERSAMPLED</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_BPP</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COMPBUFWIDTH</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MIN</name>
      <anchor>a6</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX</name>
      <anchor>a7</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>USE_3BIT_FOLD_TYPE</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DECLARE_INK</name>
      <anchor>a9</anchor>
      <arglist>(name, density)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SHADE</name>
      <anchor>a10</anchor>
      <arglist>(density, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK_K</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK_CMY</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK_CMYK</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK_CcMmYK</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK_CcMmYyK</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK_BLACK_MASK</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK_PHOTO_MASK</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_SLOT_ASF1</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_SLOT_ASF2</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_SLOT_MAN1</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_SLOT_MAN2</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_DMT</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_MSB_FIRST</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_a</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_b</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_q</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_m</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_d</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_t</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_c</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_p</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_l</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_r</name>
      <anchor>a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_g</name>
      <anchor>a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_ACKSHORT</name>
      <anchor>a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_STD0</name>
      <anchor>a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_CAP_STD1</name>
      <anchor>a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_MODES</name>
      <anchor>a38</anchor>
      <arglist>(A)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CANON_INK</name>
      <anchor>a39</anchor>
      <arglist>(A)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PUT</name>
      <anchor>a40</anchor>
      <arglist>(WHAT, VAL, RES)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ESC28</name>
      <anchor>a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ESC5b</name>
      <anchor>a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ESC40</name>
      <anchor>a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a45</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>canon_dot_sizes</type>
      <name>canon_dot_size_t</name>
      <anchor>a49</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>canon_densities</type>
      <name>canon_densities_t</name>
      <anchor>a50</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>canon_variable_ink</type>
      <name>canon_variable_ink_t</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>canon_variable_inkset</type>
      <name>canon_variable_inkset_t</name>
      <anchor>a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>canon_variable_inklist</type>
      <name>canon_variable_inklist_t</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>canon_variable_inklist_t *</type>
      <name>canon_variable_inklist_p</name>
      <anchor>a71</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>canon_caps</type>
      <name>canon_cap_t</name>
      <anchor>a79</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>colormode_t</name>
      <anchor>a156</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_MONOCHROME</name>
      <anchor>a156a94</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CMY</name>
      <anchor>a156a95</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CMYK</name>
      <anchor>a156a96</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CCMMYK</name>
      <anchor>a156a97</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>COLOR_CCMMYYK</name>
      <anchor>a156a98</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK</name>
      <anchor>a99</anchor>
      <arglist>(canon_Cc_1bit, 0.75)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK</name>
      <anchor>a100</anchor>
      <arglist>(canon_Mm_1bit, 0.75)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK</name>
      <anchor>a101</anchor>
      <arglist>(canon_X_2bit, 1.0)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK</name>
      <anchor>a102</anchor>
      <arglist>(canon_Xx_2bit, 1.0)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK</name>
      <anchor>a103</anchor>
      <arglist>(canon_X_3bit, 1.0)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DECLARE_INK</name>
      <anchor>a104</anchor>
      <arglist>(canon_Xx_3bit, 1.0)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_write_line</name>
      <anchor>a105</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const paper_t *</type>
      <name>get_media_type</name>
      <anchor>a106</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const canon_cap_t *</type>
      <name>canon_get_model_capabilities</name>
      <anchor>a107</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_source_type</name>
      <anchor>a108</anchor>
      <arglist>(const char *name, const canon_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_printhead_type</name>
      <anchor>a109</anchor>
      <arglist>(const char *name, const canon_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>colormode_t</type>
      <name>canon_printhead_colors</name>
      <anchor>a110</anchor>
      <arglist>(const char *name, const canon_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned char</type>
      <name>canon_size_type</name>
      <anchor>a111</anchor>
      <arglist>(const stp_vars_t *v, const canon_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_res_code</name>
      <anchor>a112</anchor>
      <arglist>(const canon_cap_t *caps, int xdpi, int ydpi)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_ink_type</name>
      <anchor>a113</anchor>
      <arglist>(const canon_cap_t *caps, int res_code)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>canon_lum_adjustment</name>
      <anchor>a114</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>canon_hue_adjustment</name>
      <anchor>a115</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>canon_sat_adjustment</name>
      <anchor>a116</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>canon_density</name>
      <anchor>a117</anchor>
      <arglist>(const canon_cap_t *caps, int res_code)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const canon_variable_inkset_t *</type>
      <name>canon_inks</name>
      <anchor>a118</anchor>
      <arglist>(const canon_cap_t *caps, int res_code, int colors, int bits)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_describe_resolution</name>
      <anchor>a119</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>canon_describe_output</name>
      <anchor>a120</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>canon_list_parameters</name>
      <anchor>a121</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_parameters</name>
      <anchor>a122</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>internal_imageable_area</name>
      <anchor>a123</anchor>
      <arglist>(const stp_vars_t *v, int use_paper_margins, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_imageable_area</name>
      <anchor>a124</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_limit</name>
      <anchor>a125</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_cmd</name>
      <anchor>a126</anchor>
      <arglist>(const stp_vars_t *v, const char *ini, const char cmd, int num,...)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_resetPrinter</name>
      <anchor>a127</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setPageMode</name>
      <anchor>a128</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setDataCompression</name>
      <anchor>a129</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setColor</name>
      <anchor>a130</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setResolution</name>
      <anchor>a131</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setPageMargins</name>
      <anchor>a132</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setTray</name>
      <anchor>a133</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setPrintMode</name>
      <anchor>a134</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setPageMargins2</name>
      <anchor>a135</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setPageID</name>
      <anchor>a136</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setX72</name>
      <anchor>a137</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_setImage</name>
      <anchor>a138</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_init_printer</name>
      <anchor>a139</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_deinit_printer</name>
      <anchor>a140</anchor>
      <arglist>(const stp_vars_t *v, canon_init_t *init)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_start_job</name>
      <anchor>a141</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_end_job</name>
      <anchor>a142</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_advance_buffer</name>
      <anchor>a143</anchor>
      <arglist>(unsigned char *buf, int len, int num)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_column</name>
      <anchor>a144</anchor>
      <arglist>(canon_privdata_t *privdata, int col, int buf_length)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_printfunc</name>
      <anchor>a145</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>get_double_param</name>
      <anchor>a146</anchor>
      <arglist>(stp_vars_t *v, const char *param)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_ink_ranges</name>
      <anchor>a147</anchor>
      <arglist>(stp_vars_t *v, const canon_variable_ink_t *ink, int color, const char *channel_param, const char *subchannel_param)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_do_print</name>
      <anchor>a148</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_print</name>
      <anchor>a149</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_fold_2bit</name>
      <anchor>a150</anchor>
      <arglist>(const unsigned char *line, int single_length, unsigned char *outbuf)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_fold_3bit</name>
      <anchor>a151</anchor>
      <arglist>(const unsigned char *line, int single_length, unsigned char *outbuf)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>canon_shift_buffer</name>
      <anchor>a152</anchor>
      <arglist>(unsigned char *line, int length, int bits)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>canon_write</name>
      <anchor>a153</anchor>
      <arglist>(stp_vars_t *v, const canon_cap_t *caps, unsigned char *line, int length, int coloridx, int ydpi, int *empty, int width, int offset, int bits)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_canon_module_init</name>
      <anchor>a154</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_canon_module_exit</name>
      <anchor>a155</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>channel_color_map</name>
      <anchor>a46</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>subchannel_color_map</name>
      <anchor>a47</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const double</type>
      <name>ink_darknesses</name>
      <anchor>a48</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_dotsize_t</type>
      <name>single_dotsize</name>
      <anchor>a54</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>canon_Cc_1bit_shades</name>
      <anchor>a55</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>canon_Mm_1bit_shades</name>
      <anchor>a56</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_dotsize_t</type>
      <name>two_bit_dotsize</name>
      <anchor>a57</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>canon_X_2bit_shades</name>
      <anchor>a58</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>canon_Xx_2bit_shades</name>
      <anchor>a59</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_dotsize_t</type>
      <name>three_bit_dotsize</name>
      <anchor>a60</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>canon_X_3bit_shades</name>
      <anchor>a61</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>canon_Xx_3bit_shades</name>
      <anchor>a62</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CMY_1</name>
      <anchor>a63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CMY_2</name>
      <anchor>a64</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CMYK_1</name>
      <anchor>a65</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CcMmYK_1</name>
      <anchor>a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CMYK_2</name>
      <anchor>a67</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CcMmYK_2</name>
      <anchor>a68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CMYK_3</name>
      <anchor>a69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inkset_t</type>
      <name>ci_CcMmYK_3</name>
      <anchor>a70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inklist_t</type>
      <name>canon_ink_standard</name>
      <anchor>a72</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inklist_t</type>
      <name>canon_ink_oldphoto</name>
      <anchor>a73</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inklist_t</type>
      <name>canon_ink_standardphoto</name>
      <anchor>a74</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_variable_inklist_t</type>
      <name>canon_ink_superphoto</name>
      <anchor>a75</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_sat_adjustment</name>
      <anchor>a76</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_lum_adjustment</name>
      <anchor>a77</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_hue_adjustment</name>
      <anchor>a78</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_cap_t</type>
      <name>canon_model_capabilities</name>
      <anchor>a80</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const canon_res_t</type>
      <name>canon_resolutions</name>
      <anchor>a81</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>plain_paper_lum_adjustment</name>
      <anchor>a82</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_t</type>
      <name>canon_paper_list</name>
      <anchor>a83</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>paper_type_count</name>
      <anchor>a84</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a85</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>the_parameter_count</name>
      <anchor>a86</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const float_param_t</type>
      <name>float_parameters</name>
      <anchor>a87</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>float_parameter_count</name>
      <anchor>a88</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_param_string_t</type>
      <name>media_sources</name>
      <anchor>a89</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_printfuncs_t</type>
      <name>print_canon_printfuncs</name>
      <anchor>a90</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_family_t</type>
      <name>print_canon_module_data</name>
      <anchor>a91</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a92</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a93</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-color.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-color_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="curve-cache_8h" name="curve-cache.h" local="no">gimp-print/curve-cache.h</includes>
    <includes id="color-conversion_8h" name="color-conversion.h" local="yes">color-conversion.h</includes>
    <class kind="struct">float_param_t</class>
    <class kind="struct">curve_param_t</class>
    <member kind="define">
      <type>#define</type>
      <name>RAW_GAMMA_CHANNEL</name>
      <anchor>a0</anchor>
      <arglist>(channel)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RAW_CURVE_CHANNEL</name>
      <anchor>a1</anchor>
      <arglist>(channel)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>const color_description_t *</type>
      <name>get_color_description</name>
      <anchor>a28</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const channel_depth_t *</type>
      <name>get_channel_depth</name>
      <anchor>a29</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const color_correction_t *</type>
      <name>get_color_correction</name>
      <anchor>a30</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const color_correction_t *</type>
      <name>get_color_correction_by_tag</name>
      <anchor>a31</anchor>
      <arglist>(color_correction_enum_t correction)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>initialize_channels</name>
      <anchor>a32</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_color_traditional_get_row</name>
      <anchor>a33</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int row, unsigned *zero_mask)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>free_channels</name>
      <anchor>a34</anchor>
      <arglist>(lut_t *lut)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>lut_t *</type>
      <name>allocate_lut</name>
      <anchor>a35</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void *</type>
      <name>copy_lut</name>
      <anchor>a36</anchor>
      <arglist>(void *vlut)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>free_lut</name>
      <anchor>a37</anchor>
      <arglist>(void *vlut)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_curve_t *</type>
      <name>compute_gcr_curve</name>
      <anchor>a38</anchor>
      <arglist>(const stp_vars_t *vars)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>initialize_gcr_curve</name>
      <anchor>a39</anchor>
      <arglist>(const stp_vars_t *vars)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>channel_is_synthesized</name>
      <anchor>a40</anchor>
      <arglist>(lut_t *lut, int channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_user_correction</name>
      <anchor>a41</anchor>
      <arglist>(lut_t *lut)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_a_curve_full</name>
      <anchor>a42</anchor>
      <arglist>(lut_t *lut, int channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_a_curve_fast</name>
      <anchor>a43</anchor>
      <arglist>(lut_t *lut, int channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_a_curve</name>
      <anchor>a44</anchor>
      <arglist>(lut_t *lut, int channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>invert_curve</name>
      <anchor>a45</anchor>
      <arglist>(stp_curve_t *curve, int invert_output)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compute_one_lut</name>
      <anchor>a46</anchor>
      <arglist>(lut_t *lut, int i)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_channel</name>
      <anchor>a47</anchor>
      <arglist>(stp_vars_t *v, int i, const channel_param_t *p)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_compute_lut</name>
      <anchor>a48</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_color_traditional_init</name>
      <anchor>a49</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, size_t steps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>initialize_standard_curves</name>
      <anchor>a50</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>stpi_color_traditional_list_parameters</name>
      <anchor>a51</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_color_traditional_describe_parameter</name>
      <anchor>a52</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>color_traditional_module_init</name>
      <anchor>a53</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>color_traditional_module_exit</name>
      <anchor>a54</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const color_correction_t</type>
      <name>color_corrections</name>
      <anchor>a4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>color_correction_count</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const channel_param_t</type>
      <name>channel_params</name>
      <anchor>a6</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>channel_param_count</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const channel_param_t</type>
      <name>raw_channel_params</name>
      <anchor>a8</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>raw_channel_param_count</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const color_description_t</type>
      <name>color_descriptions</name>
      <anchor>a10</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>color_description_count</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const channel_depth_t</type>
      <name>channel_depths</name>
      <anchor>a12</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>channel_depth_count</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const float_param_t</type>
      <name>float_parameters</name>
      <anchor>a14</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>float_parameter_count</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>int</type>
      <name>standard_curves_initialized</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_curve_t *</type>
      <name>hue_map_bounds</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_curve_t *</type>
      <name>lum_map_bounds</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_curve_t *</type>
      <name>sat_map_bounds</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_curve_t *</type>
      <name>color_curve_bounds</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_curve_t *</type>
      <name>gcr_curve_bounds</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>curve_param_t</type>
      <name>curve_parameters</name>
      <anchor>a22</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>curve_parameter_count</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_colorfuncs_t</type>
      <name>stpi_color_traditional_colorfuncs</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_color_t</type>
      <name>stpi_color_traditional_module_data</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-dither-matrices.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-dither-matrices_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="dither-impl_8h" name="dither-impl.h" local="yes">dither-impl.h</includes>
    <class kind="struct">stp_xml_dither_cache_t</class>
    <member kind="define">
      <type>#define</type>
      <name>MATRIX_POINT</name>
      <anchor>a0</anchor>
      <arglist>(m, x, y, x_size, y_size)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>gcd</name>
      <anchor>a2</anchor>
      <arglist>(unsigned a, unsigned b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>calc_ordered_point</name>
      <anchor>a3</anchor>
      <arglist>(unsigned x, unsigned y, int steps, int multiplier, int size, const unsigned *map)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>is_po2</name>
      <anchor>a4</anchor>
      <arglist>(size_t i)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_iterated_init</name>
      <anchor>a5</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, size_t size, size_t expt, const unsigned *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_shear</name>
      <anchor>a6</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int x_shear, int y_shear)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_dither_matrix_validate_array</name>
      <anchor>a7</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_init_from_dither_array</name>
      <anchor>a8</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, const stp_array_t *array, int transpose)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_init</name>
      <anchor>a9</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int x_size, int y_size, const unsigned int *array, int transpose, int prescaled)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_init_short</name>
      <anchor>a10</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int x_size, int y_size, const unsigned short *array, int transpose, int prescaled)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_destroy</name>
      <anchor>a11</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_clone</name>
      <anchor>a12</anchor>
      <arglist>(const stp_dither_matrix_impl_t *src, stp_dither_matrix_impl_t *dest, int x_offset, int y_offset)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_copy</name>
      <anchor>a13</anchor>
      <arglist>(const stp_dither_matrix_impl_t *src, stp_dither_matrix_impl_t *dest)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_scale_exponentially</name>
      <anchor>a14</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, double exponent)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_matrix_set_row</name>
      <anchor>a15</anchor>
      <arglist>(stp_dither_matrix_impl_t *mat, int y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>preinit_matrix</name>
      <anchor>a16</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>postinit_matrix</name>
      <anchor>a17</anchor>
      <arglist>(stp_vars_t *v, int x_shear, int y_shear)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_iterated_matrix</name>
      <anchor>a18</anchor>
      <arglist>(stp_vars_t *v, size_t edge, size_t iterations, const unsigned *data, int prescaled, int x_shear, int y_shear)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_matrix</name>
      <anchor>a19</anchor>
      <arglist>(stp_vars_t *v, const stp_dither_matrix_generic_t *matrix, int transposed, int x_shear, int y_shear)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_matrix_from_dither_array</name>
      <anchor>a20</anchor>
      <arglist>(stp_vars_t *v, const stp_array_t *array, int transpose)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dither_set_transition</name>
      <anchor>a21</anchor>
      <arglist>(stp_vars_t *v, double exponent)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_xml_dither_cache_t *</type>
      <name>stp_xml_dither_cache_get</name>
      <anchor>a22</anchor>
      <arglist>(int x, int y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stp_xml_dither_cache_set</name>
      <anchor>a23</anchor>
      <arglist>(int x, int y, const char *filename)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stp_xml_process_dither_matrix</name>
      <anchor>a24</anchor>
      <arglist>(stp_mxml_node_t *dm, const char *file)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_array_t *</type>
      <name>stpi_dither_array_create_from_xmltree</name>
      <anchor>a25</anchor>
      <arglist>(stp_mxml_node_t *dm)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_array_t *</type>
      <name>xml_doc_get_dither_array</name>
      <anchor>a26</anchor>
      <arglist>(stp_mxml_node_t *doc)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_array_t *</type>
      <name>stpi_dither_array_create_from_file</name>
      <anchor>a27</anchor>
      <arglist>(const char *file)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_array_t *</type>
      <name>stp_xml_get_dither_array</name>
      <anchor>a28</anchor>
      <arglist>(int x, int y)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_dither</name>
      <anchor>ga29</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_find_standard_dither_array</name>
      <anchor>a30</anchor>
      <arglist>(int x_aspect, int y_aspect)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_list_t *</type>
      <name>dither_matrix_cache</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-escp2-data.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-escp2-data_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="print-escp2_8h" name="print-escp2.h" local="yes">print-escp2.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>INCH</name>
      <anchor>a0</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>g1_dotsizes</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>g2_dotsizes</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>g3_dotsizes</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>c6pl_dotsizes</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>c4pl_dotsizes</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>c4pl_pigment_dotsizes</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>c3pl_dotsizes</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>c3pl_pigment_dotsizes</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>p3pl_dotsizes</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>p1_5pl_dotsizes</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>c2pl_dotsizes</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>c1_8pl_dotsizes</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sc440_dotsizes</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sc480_dotsizes</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sc600_dotsizes</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sc640_dotsizes</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sc660_dotsizes</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sc670_dotsizes</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sp700_dotsizes</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sp720_dotsizes</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>sp2000_dotsizes</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>spro_dye_dotsizes</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>spro_pigment_dotsizes</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>spro10000_dotsizes</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>spro5000_dotsizes</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_dot_size_t</type>
      <name>spro_c4pl_pigment_dotsizes</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_bits_t</type>
      <name>variable_bits</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_bits_t</type>
      <name>stp950_bits</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_bits_t</type>
      <name>ultrachrome_bits</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_bits_t</type>
      <name>standard_bits</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_bits_t</type>
      <name>c1_8_bits</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>standard_base_res</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>g3_base_res</name>
      <anchor>a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>variable_base_res</name>
      <anchor>a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>stp950_base_res</name>
      <anchor>a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>ultrachrome_base_res</name>
      <anchor>a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>c1_8_base_res</name>
      <anchor>a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>c1_5_base_res</name>
      <anchor>a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>stc900_base_res</name>
      <anchor>a39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_base_resolutions_t</type>
      <name>pro_base_res</name>
      <anchor>a40</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>g1_densities</name>
      <anchor>a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>g3_densities</name>
      <anchor>a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c6pl_densities</name>
      <anchor>a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c4pl_2880_densities</name>
      <anchor>a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c4pl_densities</name>
      <anchor>a45</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c4pl_pigment_densities</name>
      <anchor>a46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c3pl_pigment_densities</name>
      <anchor>a47</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c3pl_densities</name>
      <anchor>a48</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>p3pl_densities</name>
      <anchor>a49</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>p1_5pl_densities</name>
      <anchor>a50</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c2pl_densities</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>c1_8pl_densities</name>
      <anchor>a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>sc1500_densities</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>sc440_densities</name>
      <anchor>a54</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>sc480_densities</name>
      <anchor>a55</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>sc660_densities</name>
      <anchor>a56</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>sc980_densities</name>
      <anchor>a57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>sp700_densities</name>
      <anchor>a58</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>sp2000_densities</name>
      <anchor>a59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>spro_dye_densities</name>
      <anchor>a60</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>spro_pigment_densities</name>
      <anchor>a61</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_densities_t</type>
      <name>spro10000_densities</name>
      <anchor>a62</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_t</type>
      <name>standard_roll_feed_input_slots</name>
      <anchor>a63</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_list_t</type>
      <name>standard_roll_feed_input_slot_list</name>
      <anchor>a64</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_t</type>
      <name>cutter_roll_feed_input_slots</name>
      <anchor>a65</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_list_t</type>
      <name>cutter_roll_feed_input_slot_list</name>
      <anchor>a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_t</type>
      <name>cd_cutter_roll_feed_input_slots</name>
      <anchor>a67</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_list_t</type>
      <name>cd_cutter_roll_feed_input_slot_list</name>
      <anchor>a68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_t</type>
      <name>cd_roll_feed_input_slots</name>
      <anchor>a69</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_list_t</type>
      <name>cd_roll_feed_input_slot_list</name>
      <anchor>a70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_t</type>
      <name>pro_roll_feed_input_slots</name>
      <anchor>a71</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_list_t</type>
      <name>pro_roll_feed_input_slot_list</name>
      <anchor>a72</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_t</type>
      <name>spro5000_input_slots</name>
      <anchor>a73</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_list_t</type>
      <name>spro5000_input_slot_list</name>
      <anchor>a74</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const input_slot_list_t</type>
      <name>default_input_slot_list</name>
      <anchor>a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_raw_t</type>
      <name>new_init_sequence</name>
      <anchor>a76</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_raw_t</type>
      <name>je_deinit_sequence</name>
      <anchor>a77</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const quality_t</type>
      <name>standard_qualities</name>
      <anchor>a78</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const quality_list_t</type>
      <name>standard_quality_list</name>
      <anchor>a79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stpi_escp2_printer_t</type>
      <name>stpi_escp2_model_capabilities</name>
      <anchor>a80</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>stpi_escp2_model_limit</name>
      <anchor>a81</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-escp2.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-escp2_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="print-escp2_8h" name="print-escp2.h" local="yes">print-escp2.h</includes>
    <class kind="struct">escp2_printer_attr_t</class>
    <class kind="struct">channel_count_t</class>
    <class kind="struct">float_param_t</class>
    <member kind="define">
      <type>#define</type>
      <name>OP_JOB_START</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OP_JOB_PRINT</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OP_JOB_END</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX</name>
      <anchor>a3</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>INCH</name>
      <anchor>a4</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PARAMETER_INT</name>
      <anchor>a5</anchor>
      <arglist>(s)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PARAMETER_INT_RO</name>
      <anchor>a6</anchor>
      <arglist>(s)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PARAMETER_RAW</name>
      <anchor>a7</anchor>
      <arglist>(s)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEF_SIMPLE_ACCESSOR</name>
      <anchor>a8</anchor>
      <arglist>(f, t)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEF_RAW_ACCESSOR</name>
      <anchor>a9</anchor>
      <arglist>(f, t)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEF_COMPOSITE_ACCESSOR</name>
      <anchor>a10</anchor>
      <arglist>(f, t)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEF_ROLL_ACCESSOR</name>
      <anchor>a11</anchor>
      <arglist>(f, t)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>const res_t *</type>
      <name>escp2_find_resolution</name>
      <anchor>a26</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>escp2_privdata_t *</type>
      <name>get_privdata</name>
      <anchor>a27</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>model_featureset_t</type>
      <name>escp2_get_cap</name>
      <anchor>a28</anchor>
      <arglist>(const stp_vars_t *v, escp2_model_option_t feature)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_has_cap</name>
      <anchor>a29</anchor>
      <arglist>(const stp_vars_t *v, escp2_model_option_t feature, model_featureset_t class)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const channel_count_t *</type>
      <name>get_channel_count_by_name</name>
      <anchor>a30</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const channel_count_t *</type>
      <name>get_channel_count_by_number</name>
      <anchor>a31</anchor>
      <arglist>(unsigned count)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_ink_type</name>
      <anchor>a32</anchor>
      <arglist>(const stp_vars_t *v, int resid)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>escp2_density</name>
      <anchor>a33</anchor>
      <arglist>(const stp_vars_t *v, int resid)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_bits</name>
      <anchor>a34</anchor>
      <arglist>(const stp_vars_t *v, int resid)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>escp2_base_res</name>
      <anchor>a35</anchor>
      <arglist>(const stp_vars_t *v, int resid)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const escp2_dropsize_t *</type>
      <name>escp2_dropsizes</name>
      <anchor>a36</anchor>
      <arglist>(const stp_vars_t *v, int resid)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const inklist_t *</type>
      <name>escp2_inklist</name>
      <anchor>a37</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const shade_t *</type>
      <name>escp2_shades</name>
      <anchor>a38</anchor>
      <arglist>(const stp_vars_t *v, int channel)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const paperlist_t *</type>
      <name>escp2_paperlist</name>
      <anchor>a39</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>using_automatic_settings</name>
      <anchor>a40</anchor>
      <arglist>(const stp_vars_t *v, auto_mode_t mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>compute_internal_resid</name>
      <anchor>a41</anchor>
      <arglist>(int hres, int vres)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>compute_resid</name>
      <anchor>a42</anchor>
      <arglist>(const res_t *res)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>compute_printed_resid</name>
      <anchor>a43</anchor>
      <arglist>(const res_t *res)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const input_slot_t *</type>
      <name>get_input_slot</name>
      <anchor>a44</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const printer_weave_t *</type>
      <name>get_printer_weave</name>
      <anchor>a45</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>use_printer_weave</name>
      <anchor>a46</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const paper_t *</type>
      <name>get_media_type</name>
      <anchor>a47</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_resolution_by_paper_type</name>
      <anchor>a48</anchor>
      <arglist>(const stp_vars_t *v, const res_t *res)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_resolution</name>
      <anchor>a49</anchor>
      <arglist>(const stp_vars_t *v, const res_t *res)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_papersize</name>
      <anchor>a50</anchor>
      <arglist>(const stp_vars_t *v, const stp_papersize_t *pt)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_inktype</name>
      <anchor>a51</anchor>
      <arglist>(const stp_vars_t *v, const escp2_inkname_t *inks)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>get_default_inktype</name>
      <anchor>a52</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const escp2_inkname_t *</type>
      <name>get_inktype</name>
      <anchor>a53</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const paper_adjustment_t *</type>
      <name>get_media_adjustment</name>
      <anchor>a54</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>escp2_list_parameters</name>
      <anchor>a55</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>fill_transition_parameters</name>
      <anchor>a56</anchor>
      <arglist>(stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_density_parameter</name>
      <anchor>a57</anchor>
      <arglist>(const stp_vars_t *v, stp_parameter_t *description, int color)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_gray_transition_parameter</name>
      <anchor>a58</anchor>
      <arglist>(const stp_vars_t *v, stp_parameter_t *description, int expected_channels)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_color_transition_parameter</name>
      <anchor>a59</anchor>
      <arglist>(const stp_vars_t *v, stp_parameter_t *description, int color)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const res_t *</type>
      <name>find_default_resolution</name>
      <anchor>a60</anchor>
      <arglist>(const stp_vars_t *v, int desired_hres, int desired_vres, int strict)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const res_t *</type>
      <name>find_resolution_from_quality</name>
      <anchor>a61</anchor>
      <arglist>(const stp_vars_t *v, const char *quality, int strict)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_parameters</name>
      <anchor>a62</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>imax</name>
      <anchor>a63</anchor>
      <arglist>(int a, int b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>internal_imageable_area</name>
      <anchor>a64</anchor>
      <arglist>(const stp_vars_t *v, int use_paper_margins, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_imageable_area</name>
      <anchor>a65</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_limit</name>
      <anchor>a66</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>escp2_describe_resolution</name>
      <anchor>a67</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>escp2_describe_output</name>
      <anchor>a68</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_has_advanced_command_set</name>
      <anchor>a69</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_use_extended_commands</name>
      <anchor>a70</anchor>
      <arglist>(const stp_vars_t *v, int use_softweave)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>set_raw_ink_type</name>
      <anchor>a71</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>adjust_density_and_ink_type</name>
      <anchor>a72</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>adjust_print_quality</name>
      <anchor>a73</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>count_channels</name>
      <anchor>a74</anchor>
      <arglist>(const escp2_inkname_t *inks)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>compute_channel_count</name>
      <anchor>a75</anchor>
      <arglist>(const escp2_inkname_t *ink_type, int channel_limit)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>get_double_param</name>
      <anchor>a76</anchor>
      <arglist>(const stp_vars_t *v, const char *param)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_inks</name>
      <anchor>a77</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_head_offset</name>
      <anchor>a78</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_misc</name>
      <anchor>a79</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>allocate_channels</name>
      <anchor>a80</anchor>
      <arglist>(stp_vars_t *v, int line_length)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>gcd</name>
      <anchor>a81</anchor>
      <arglist>(unsigned a, unsigned b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned</type>
      <name>lcm</name>
      <anchor>a82</anchor>
      <arglist>(unsigned a, unsigned b)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>adjusted_vertical_resolution</name>
      <anchor>a83</anchor>
      <arglist>(const res_t *res)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>adjusted_horizontal_resolution</name>
      <anchor>a84</anchor>
      <arglist>(const res_t *res)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_resolution</name>
      <anchor>a85</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_softweave_parameters</name>
      <anchor>a86</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_printer_weave_parameters</name>
      <anchor>a87</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_head_parameters</name>
      <anchor>a88</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>setup_page</name>
      <anchor>a89</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_mask</name>
      <anchor>a90</anchor>
      <arglist>(unsigned char *cd_mask, int x_center, int scaled_x_where, int limit, int expansion, int invert)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_print_data</name>
      <anchor>a91</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_print_page</name>
      <anchor>a92</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_do_print</name>
      <anchor>a93</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int print_op)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_print</name>
      <anchor>a94</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_job_start</name>
      <anchor>a95</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>escp2_job_end</name>
      <anchor>a96</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_escp2_module_init</name>
      <anchor>a97</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_escp2_module_exit</name>
      <anchor>a98</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const escp2_printer_attr_t</type>
      <name>escp2_printer_attrs</name>
      <anchor>a14</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const channel_count_t</type>
      <name>escp2_channel_counts</name>
      <anchor>a15</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>int</type>
      <name>escp2_channel_counts_count</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const double</type>
      <name>ink_darknesses</name>
      <anchor>a17</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a18</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>the_parameter_count</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const float_param_t</type>
      <name>float_parameters</name>
      <anchor>a20</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>float_parameter_count</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_printfuncs_t</type>
      <name>print_escp2_printfuncs</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_family_t</type>
      <name>print_escp2_module_data</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-escp2.h</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-escp2_8h</filename>
    <class kind="struct">escp2_dropsize_t</class>
    <class kind="struct">paper_adjustment_t</class>
    <class kind="struct">paper_adjustment_list_t</class>
    <class kind="struct">paper_t</class>
    <class kind="struct">paperlist_t</class>
    <class kind="struct">res_t</class>
    <class kind="struct">physical_subchannel_t</class>
    <class kind="struct">ink_channel_t</class>
    <class kind="struct">channel_set_t</class>
    <class kind="struct">escp2_inkname_t</class>
    <class kind="struct">shade_t</class>
    <class kind="struct">inklist_t</class>
    <class kind="struct">inkgroup_t</class>
    <class kind="struct">input_slot_t</class>
    <class kind="struct">input_slot_list_t</class>
    <class kind="struct">quality_t</class>
    <class kind="struct">quality_list_t</class>
    <class kind="struct">printer_weave_t</class>
    <class kind="struct">printer_weave_list_t</class>
    <class kind="struct">escp2_printer</class>
    <class kind="struct">escp2_privdata_t</class>
    <member kind="define">
      <type>#define</type>
      <name>PHYSICAL_CHANNEL_LIMIT</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX_DROP_SIZES</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>XCOLOR_R</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>XCOLOR_B</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>XCOLOR_GLOSS</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_LOW</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_360</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_720_360</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_720</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_1440_720</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_2880_720</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_2880_1440</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_2880_2880</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>RES_N</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ROLL_FEED_CUT_ALL</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ROLL_FEED_CUT_LAST</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ROLL_FEED_DONT_EJECT</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_COMMAND_MASK</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_COMMAND_1998</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_COMMAND_1999</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_COMMAND_2000</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_COMMAND_PRO</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_XZEROMARGIN_MASK</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_XZEROMARGIN_NO</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_XZEROMARGIN_YES</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_ROLLFEED_MASK</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_ROLLFEED_NO</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_ROLLFEED_YES</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_VARIABLE_DOT_MASK</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_VARIABLE_NO</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_VARIABLE_YES</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_GRAYMODE_MASK</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_GRAYMODE_NO</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_GRAYMODE_YES</name>
      <anchor>a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_VACUUM_MASK</name>
      <anchor>a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_VACUUM_NO</name>
      <anchor>a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_VACUUM_YES</name>
      <anchor>a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_FAST_360_MASK</name>
      <anchor>a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_FAST_360_NO</name>
      <anchor>a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_FAST_360_YES</name>
      <anchor>a39</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_SEND_ZERO_ADVANCE_MASK</name>
      <anchor>a40</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_SEND_ZERO_ADVANCE_NO</name>
      <anchor>a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_SEND_ZERO_ADVANCE_YES</name>
      <anchor>a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_SUPPORTS_INK_CHANGE_MASK</name>
      <anchor>a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_SUPPORTS_INK_CHANGE_NO</name>
      <anchor>a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_SUPPORTS_INK_CHANGE_YES</name>
      <anchor>a45</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_PACKET_MODE_MASK</name>
      <anchor>a46</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_PACKET_MODE_NO</name>
      <anchor>a47</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_PACKET_MODE_YES</name>
      <anchor>a48</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_PRINT_TO_CD_MASK</name>
      <anchor>a49</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_PRINT_TO_CD_NO</name>
      <anchor>a50</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MODEL_PRINT_TO_CD_YES</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COMPRESSION</name>
      <anchor>a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>FILLFUNC</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COMPUTEFUNC</name>
      <anchor>a54</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PACKFUNC</name>
      <anchor>a55</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>unsigned long</type>
      <name>model_cap_t</name>
      <anchor>a56</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>unsigned long</type>
      <name>model_featureset_t</name>
      <anchor>a57</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>const escp2_dropsize_t *const </type>
      <name>escp2_drop_list_t</name>
      <anchor>a58</anchor>
      <arglist>[RES_N]</arglist>
    </member>
    <member kind="typedef">
      <type>shade_t</type>
      <name>shade_set_t</name>
      <anchor>a59</anchor>
      <arglist>[PHYSICAL_CHANNEL_LIMIT]</arglist>
    </member>
    <member kind="typedef">
      <type>short</type>
      <name>escp2_dot_size_t</name>
      <anchor>a60</anchor>
      <arglist>[RES_N]</arglist>
    </member>
    <member kind="typedef">
      <type>short</type>
      <name>escp2_bits_t</name>
      <anchor>a61</anchor>
      <arglist>[RES_N]</arglist>
    </member>
    <member kind="typedef">
      <type>short</type>
      <name>escp2_base_resolutions_t</name>
      <anchor>a62</anchor>
      <arglist>[RES_N]</arglist>
    </member>
    <member kind="typedef">
      <type>float</type>
      <name>escp2_densities_t</name>
      <anchor>a63</anchor>
      <arglist>[RES_N]</arglist>
    </member>
    <member kind="typedef">
      <type>escp2_printer</type>
      <name>stpi_escp2_printer_t</name>
      <anchor>a64</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>paper_class_t</name>
      <anchor>a155</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPER_PLAIN</name>
      <anchor>a155a125</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPER_GOOD</name>
      <anchor>a155a126</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPER_PHOTO</name>
      <anchor>a155a127</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPER_PREMIUM_PHOTO</name>
      <anchor>a155a128</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPER_TRANSPARENCY</name>
      <anchor>a155a129</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>inkset_id_t</name>
      <anchor>a156</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INKSET_CMYK</name>
      <anchor>a156a130</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INKSET_CcMmYK</name>
      <anchor>a156a131</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INKSET_CcMmYyK</name>
      <anchor>a156a132</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INKSET_CcMmYKk</name>
      <anchor>a156a133</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INKSET_QUADTONE</name>
      <anchor>a156a134</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INKSET_CMYKRB</name>
      <anchor>a156a135</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INKSET_EXTENDED</name>
      <anchor>a156a136</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>auto_mode_t</name>
      <anchor>a157</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>AUTO_MODE_QUALITY</name>
      <anchor>a157a137</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>AUTO_MODE_MANUAL</name>
      <anchor>a157a138</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>escp2_model_option_t</name>
      <anchor>a158</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_COMMAND</name>
      <anchor>a158a139</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_XZEROMARGIN</name>
      <anchor>a158a140</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_ROLLFEED</name>
      <anchor>a158a141</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_VARIABLE_DOT</name>
      <anchor>a158a142</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_GRAYMODE</name>
      <anchor>a158a143</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_VACUUM</name>
      <anchor>a158a144</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_FAST_360</name>
      <anchor>a158a145</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_SEND_ZERO_ADVANCE</name>
      <anchor>a158a146</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_SUPPORTS_INK_CHANGE</name>
      <anchor>a158a147</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_PACKET_MODE</name>
      <anchor>a158a148</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_PRINT_TO_CD</name>
      <anchor>a158a149</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_LIMIT</name>
      <anchor>a158a150</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_init_printer</name>
      <anchor>a151</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_deinit_printer</name>
      <anchor>a152</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_flush_pass</name>
      <anchor>a153</anchor>
      <arglist>(stp_vars_t *v, int passno, int vertical_subpass)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_escp2_terminate_page</name>
      <anchor>a154</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="variable">
      <type>const stpi_escp2_printer_t</type>
      <name>stpi_escp2_model_capabilities</name>
      <anchor>a65</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>stpi_escp2_model_limit</name>
      <anchor>a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_simple_drops</name>
      <anchor>a67</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_spro10000_drops</name>
      <anchor>a68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_1_5pl_drops</name>
      <anchor>a69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_2pl_drops</name>
      <anchor>a70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_3pl_drops</name>
      <anchor>a71</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_3pl_pigment_drops</name>
      <anchor>a72</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_3pl_pmg_drops</name>
      <anchor>a73</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_1440_4pl_drops</name>
      <anchor>a74</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_ultrachrome_drops</name>
      <anchor>a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_2880_4pl_drops</name>
      <anchor>a76</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_6pl_drops</name>
      <anchor>a77</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_2000p_drops</name>
      <anchor>a78</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t</type>
      <name>stpi_escp2_variable_x80_6pl_drops</name>
      <anchor>a79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paperlist_t</type>
      <name>stpi_escp2_standard_paper_list</name>
      <anchor>a80</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paperlist_t</type>
      <name>stpi_escp2_durabrite_paper_list</name>
      <anchor>a81</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paperlist_t</type>
      <name>stpi_escp2_ultrachrome_paper_list</name>
      <anchor>a82</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_standard_paper_adjustment_list</name>
      <anchor>a83</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_durabrite_paper_adjustment_list</name>
      <anchor>a84</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_photo_paper_adjustment_list</name>
      <anchor>a85</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_photo2_paper_adjustment_list</name>
      <anchor>a86</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_photo3_paper_adjustment_list</name>
      <anchor>a87</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_sp960_paper_adjustment_list</name>
      <anchor>a88</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_ultrachrome_photo_paper_adjustment_list</name>
      <anchor>a89</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t</type>
      <name>stpi_escp2_ultrachrome_matte_paper_adjustment_list</name>
      <anchor>a90</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_superfine_reslist</name>
      <anchor>a91</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_no_printer_weave_reslist</name>
      <anchor>a92</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_pro_reslist</name>
      <anchor>a93</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_sp5000_reslist</name>
      <anchor>a94</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_720dpi_reslist</name>
      <anchor>a95</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_720dpi_soft_reslist</name>
      <anchor>a96</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_g3_720dpi_reslist</name>
      <anchor>a97</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_1440dpi_reslist</name>
      <anchor>a98</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_2880dpi_reslist</name>
      <anchor>a99</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_2880_1440dpi_reslist</name>
      <anchor>a100</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_g3_reslist</name>
      <anchor>a101</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_sc500_reslist</name>
      <anchor>a102</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const </type>
      <name>stpi_escp2_sc640_reslist</name>
      <anchor>a103</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_cmy_inkgroup</name>
      <anchor>a104</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_standard_inkgroup</name>
      <anchor>a105</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_c80_inkgroup</name>
      <anchor>a106</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_c64_inkgroup</name>
      <anchor>a107</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_x80_inkgroup</name>
      <anchor>a108</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_photo_gen1_inkgroup</name>
      <anchor>a109</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_photo_gen2_inkgroup</name>
      <anchor>a110</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_photo_gen3_inkgroup</name>
      <anchor>a111</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_photo_pigment_inkgroup</name>
      <anchor>a112</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_photo7_japan_inkgroup</name>
      <anchor>a113</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_ultrachrome_inkgroup</name>
      <anchor>a114</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_f360_photo_inkgroup</name>
      <anchor>a115</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_f360_photo7_japan_inkgroup</name>
      <anchor>a116</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_f360_ultrachrome_inkgroup</name>
      <anchor>a117</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t</type>
      <name>stpi_escp2_cmykrb_inkgroup</name>
      <anchor>a118</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_inkname_t</type>
      <name>stpi_escp2_default_black_inkset</name>
      <anchor>a119</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_list_t</type>
      <name>stpi_escp2_standard_printer_weave_list</name>
      <anchor>a120</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_list_t</type>
      <name>stpi_escp2_sp2200_printer_weave_list</name>
      <anchor>a121</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_list_t</type>
      <name>stpi_escp2_pro7000_printer_weave_list</name>
      <anchor>a122</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_list_t</type>
      <name>stpi_escp2_pro7500_printer_weave_list</name>
      <anchor>a123</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_list_t</type>
      <name>stpi_escp2_pro7600_printer_weave_list</name>
      <anchor>a124</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-lexmark.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-lexmark_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <class kind="union">lexmark_lineoff_t</class>
    <class kind="union">lexmark_linebufs_t</class>
    <class kind="struct">float_param_t</class>
    <class kind="struct">lexmark_res_t</class>
    <class kind="struct">lexmark_inkparam_t</class>
    <class kind="struct">lexmark_inkname_t</class>
    <class kind="struct">lexmark_cap_t</class>
    <class kind="struct">lexm_privdata_weave</class>
    <class kind="struct">paper_t</class>
    <class kind="struct">Lexmark_head_colors</class>
    <member kind="define">
      <type>#define</type>
      <name>USEEPSEWAVE</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_ECOLOR_LC</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_ECOLOR_LM</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_ECOLOR_LY</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>false</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>true</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>max</name>
      <anchor>a6</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>INCH</name>
      <anchor>a7</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>NCHANNELS</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DPI300</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DPI600</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DPI1200</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DPI2400</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DPItest</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>V_NOZZLE_MASK</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>H_NOZZLE_MASK</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>NOZZLE_MASK</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PRINT_MODE_300</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PRINT_MODE_600</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PRINT_MODE_1200</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PRINT_MODE_2400</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_K</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_C</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_Y</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_M</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_LC</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_LY</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_LM</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_CMYK</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_CMY</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_CcMcYK</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_CcMcY</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_MASK</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PRINT_MODE_MASK</name>
      <anchor>a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>COLOR_MODE_PHOTO</name>
      <anchor>a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BWR</name>
      <anchor>a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BWL</name>
      <anchor>a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CR</name>
      <anchor>a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CL</name>
      <anchor>a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_K</name>
      <anchor>a39</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_CMY</name>
      <anchor>a40</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_CMYK</name>
      <anchor>a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_CcMmYK</name>
      <anchor>a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_CcMmYy</name>
      <anchor>a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_CcMmYyK</name>
      <anchor>a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_BLACK_MASK</name>
      <anchor>a45</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_INK_PHOTO_MASK</name>
      <anchor>a46</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_SLOT_ASF1</name>
      <anchor>a47</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_SLOT_ASF2</name>
      <anchor>a48</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_SLOT_MAN1</name>
      <anchor>a49</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_SLOT_MAN2</name>
      <anchor>a50</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_CAP_DMT</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_CAP_MSB_FIRST</name>
      <anchor>a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_CAP_CMD61</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_CAP_CMD6d</name>
      <anchor>a54</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_CAP_CMD70</name>
      <anchor>a55</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXMARK_CAP_CMD72</name>
      <anchor>a56</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LEXM_RES_COUNT</name>
      <anchor>a57</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_300_DPI</name>
      <anchor>a58</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_600_DPI</name>
      <anchor>a59</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_1200_DPI</name>
      <anchor>a60</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_2400_DPI</name>
      <anchor>a61</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_COLOR_PRINT</name>
      <anchor>a62</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_BLACK_PRINT</name>
      <anchor>a63</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_PSHIFT</name>
      <anchor>a64</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_COLOR_MODE_POS</name>
      <anchor>a65</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_RESOLUTION_POS</name>
      <anchor>a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LX_Z52_PRINT_DIRECTION_POS</name>
      <anchor>a67</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM_Z52_HEADERSIZE</name>
      <anchor>a68</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM_Z42_HEADERSIZE</name>
      <anchor>a69</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM3200_LEFTOFFS</name>
      <anchor>a70</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM3200_RIGHTOFFS</name>
      <anchor>a71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM_3200_HEADERSIZE</name>
      <anchor>a72</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM_Z52_STARTSIZE</name>
      <anchor>a73</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM_Z42_STARTSIZE</name>
      <anchor>a74</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>ESC2a</name>
      <anchor>a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>LXM_3200_STARTSIZE</name>
      <anchor>a76</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a77</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a78</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>enum Lex_model</type>
      <name>Lex_model</name>
      <anchor>a81</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>lexmark_res_t</type>
      <name>lexmark_res_t_array</name>
      <anchor>a94</anchor>
      <arglist>[LEXM_RES_COUNT]</arglist>
    </member>
    <member kind="typedef">
      <type>lexm_privdata_weave</type>
      <name>lexm_privdata_weave</name>
      <anchor>a106</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>Lexmark_head_colors</type>
      <name>Lexmark_head_colors</name>
      <anchor>a111</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>Lex_model</name>
      <anchor>a152</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>m_lex7500</name>
      <anchor>a152a115</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>m_z52</name>
      <anchor>a152a116</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>m_z42</name>
      <anchor>a152a117</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>m_3200</name>
      <anchor>a152a118</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>flush_pass</name>
      <anchor>a119</anchor>
      <arglist>(stp_vars_t *v, int passno, int vertical_subpass)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>get_lr_shift</name>
      <anchor>a120</anchor>
      <arglist>(int mode)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_calc_3200_checksum</name>
      <anchor>a121</anchor>
      <arglist>(unsigned char *data)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>model_to_index</name>
      <anchor>a122</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const lexmark_cap_t *</type>
      <name>lexmark_get_model_capabilities</name>
      <anchor>a123</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const lexmark_inkname_t *</type>
      <name>lexmark_get_ink_type</name>
      <anchor>a124</anchor>
      <arglist>(const char *name, int printing_color, const lexmark_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const lexmark_inkparam_t *</type>
      <name>lexmark_get_ink_parameter</name>
      <anchor>a125</anchor>
      <arglist>(const char *name, int printing_color, const lexmark_cap_t *caps, const stp_vars_t *nv)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const paper_t *</type>
      <name>get_media_type</name>
      <anchor>a126</anchor>
      <arglist>(const char *name, const lexmark_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_source_type</name>
      <anchor>a127</anchor>
      <arglist>(const char *name, const lexmark_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const lexmark_lineoff_t *</type>
      <name>lexmark_head_offset</name>
      <anchor>a128</anchor>
      <arglist>(int ydpi, const char *ink_type, const lexmark_cap_t *caps, const lexmark_inkparam_t *ink_parameter, lexmark_lineoff_t *lineoff_buffer)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_get_phys_resolution_vertical</name>
      <anchor>a129</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const lexmark_res_t *</type>
      <name>lexmark_get_resolution_para</name>
      <anchor>a130</anchor>
      <arglist>(int model, const char *resolution)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_print_bidirectional</name>
      <anchor>a131</anchor>
      <arglist>(int model, const char *resolution)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>lexmark_lum_adjustment</name>
      <anchor>a132</anchor>
      <arglist>(const lexmark_cap_t *caps, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>lexmark_hue_adjustment</name>
      <anchor>a133</anchor>
      <arglist>(const lexmark_cap_t *caps, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>lexmark_sat_adjustment</name>
      <anchor>a134</anchor>
      <arglist>(const lexmark_cap_t *caps, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>lexmark_describe_resolution</name>
      <anchor>a135</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>lexmark_list_parameters</name>
      <anchor>a136</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>lexmark_describe_output</name>
      <anchor>a137</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>lexmark_parameters</name>
      <anchor>a138</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>internal_imageable_area</name>
      <anchor>a139</anchor>
      <arglist>(const stp_vars_t *v, int use_paper_margins, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>lexmark_imageable_area</name>
      <anchor>a140</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>lexmark_limit</name>
      <anchor>a141</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_init_printer</name>
      <anchor>a142</anchor>
      <arglist>(const stp_vars_t *v, const lexmark_cap_t *caps, int printing_color, const char *source_str, int xdpi, int ydpi, int page_width, int page_height, int top, int left, int use_dmt)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>lexmark_deinit_printer</name>
      <anchor>a143</anchor>
      <arglist>(const stp_vars_t *v, const lexmark_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>paper_shift</name>
      <anchor>a144</anchor>
      <arglist>(const stp_vars_t *v, int offset, const lexmark_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>get_double_param</name>
      <anchor>a145</anchor>
      <arglist>(stp_vars_t *v, const char *param)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_do_print</name>
      <anchor>a146</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_print</name>
      <anchor>a147</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned char *</type>
      <name>lexmark_init_line</name>
      <anchor>a148</anchor>
      <arglist>(int mode, unsigned char *prnBuf, int pass_length, int offset, int width, int direction, const lexmark_inkparam_t *ink_parameter, const lexmark_cap_t *caps)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>lexmark_write</name>
      <anchor>a149</anchor>
      <arglist>(const stp_vars_t *v, unsigned char *prnBuf, int *paperShift, int direction, int pass_length, const lexmark_cap_t *caps, const lexmark_inkparam_t *ink_parameter, int xdpi, int yCount, Lexmark_head_colors *head_colors, int length, int mode, int ydpi, int width, int offset, int dmt)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_lexmark_module_init</name>
      <anchor>a150</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_lexmark_module_exit</name>
      <anchor>a151</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_dotsize_t</type>
      <name>single_dotsize</name>
      <anchor>a79</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>photo_dither_shades</name>
      <anchor>a80</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_sat_adjustment</name>
      <anchor>a82</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_lum_adjustment</name>
      <anchor>a83</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_hue_adjustment</name>
      <anchor>a84</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>lr_shift_color</name>
      <anchor>a85</anchor>
      <arglist>[10]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>lr_shift_black</name>
      <anchor>a86</anchor>
      <arglist>[10]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a87</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>the_parameter_count</name>
      <anchor>a88</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const float_param_t</type>
      <name>float_parameters</name>
      <anchor>a89</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>float_parameter_count</name>
      <anchor>a90</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>head_offset_cmyk</name>
      <anchor>a91</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>head_offset_cmy</name>
      <anchor>a92</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>head_offset_cCmMyk</name>
      <anchor>a93</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>IDX_SEQLEN</name>
      <anchor>a95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const unsigned char</type>
      <name>outbufHeader_z52</name>
      <anchor>a96</anchor>
      <arglist>[LXM_Z52_HEADERSIZE]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const unsigned char</type>
      <name>outbufHeader_z42</name>
      <anchor>a97</anchor>
      <arglist>[LXM_Z42_HEADERSIZE]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const lexmark_res_t_array</type>
      <name>lexmark_reslist_z52</name>
      <anchor>a98</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const lexmark_inkname_t</type>
      <name>ink_types_z52</name>
      <anchor>a99</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>int</type>
      <name>lxm3200_headpos</name>
      <anchor>a100</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>int</type>
      <name>lxm3200_linetoeject</name>
      <anchor>a101</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>outbufHeader_3200</name>
      <anchor>a102</anchor>
      <arglist>[LXM_3200_HEADERSIZE]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const lexmark_res_t_array</type>
      <name>lexmark_reslist_3200</name>
      <anchor>a103</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const lexmark_inkname_t</type>
      <name>ink_types_3200</name>
      <anchor>a104</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const lexmark_cap_t</type>
      <name>lexmark_model_capabilities</name>
      <anchor>a105</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const paper_t</type>
      <name>lexmark_paper_list</name>
      <anchor>a107</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>paper_type_count</name>
      <anchor>a108</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_param_string_t</type>
      <name>media_sources</name>
      <anchor>a109</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_printfuncs_t</type>
      <name>print_lexmark_printfuncs</name>
      <anchor>a110</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_family_t</type>
      <name>print_lexmark_module_data</name>
      <anchor>a112</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a113</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a114</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-list.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-list_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stp_list_item</class>
    <class kind="struct">stp_list</class>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_name_cache</name>
      <anchor>a0</anchor>
      <arglist>(stp_list_t *list, const char *name, stp_list_item_t *cache)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_long_name_cache</name>
      <anchor>a1</anchor>
      <arglist>(stp_list_t *list, const char *long_name, stp_list_item_t *cache)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>clear_cache</name>
      <anchor>a2</anchor>
      <arglist>(stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_node_free_data</name>
      <anchor>a3</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>null_list</name>
      <anchor>a4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_list</name>
      <anchor>a5</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_item_t *</type>
      <name>get_start_internal</name>
      <anchor>a6</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_item_t *</type>
      <name>get_end_internal</name>
      <anchor>a7</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_list_create</name>
      <anchor>a8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_t *</type>
      <name>stp_list_copy</name>
      <anchor>a9</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_destroy</name>
      <anchor>a10</anchor>
      <arglist>(stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_get_length</name>
      <anchor>a11</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_start</name>
      <anchor>a12</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_end</name>
      <anchor>a13</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_index</name>
      <anchor>a14</anchor>
      <arglist>(const stp_list_t *list, int idx)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_name_internal</name>
      <anchor>a15</anchor>
      <arglist>(const stp_list_t *list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_name</name>
      <anchor>a16</anchor>
      <arglist>(const stp_list_t *list, const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_long_name_internal</name>
      <anchor>a17</anchor>
      <arglist>(const stp_list_t *list, const char *long_name)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_get_item_by_long_name</name>
      <anchor>a18</anchor>
      <arglist>(const stp_list_t *list, const char *long_name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_freefunc</name>
      <anchor>a19</anchor>
      <arglist>(stp_list_t *list, stp_node_freefunc freefunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_freefunc</type>
      <name>stp_list_get_freefunc</name>
      <anchor>a20</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_copyfunc</name>
      <anchor>a21</anchor>
      <arglist>(stp_list_t *list, stp_node_copyfunc copyfunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_copyfunc</type>
      <name>stp_list_get_copyfunc</name>
      <anchor>a22</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_namefunc</name>
      <anchor>a23</anchor>
      <arglist>(stp_list_t *list, stp_node_namefunc namefunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_namefunc</type>
      <name>stp_list_get_namefunc</name>
      <anchor>a24</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_long_namefunc</name>
      <anchor>a25</anchor>
      <arglist>(stp_list_t *list, stp_node_namefunc long_namefunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_namefunc</type>
      <name>stp_list_get_long_namefunc</name>
      <anchor>a26</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_list_set_sortfunc</name>
      <anchor>a27</anchor>
      <arglist>(stp_list_t *list, stp_node_sortfunc sortfunc)</arglist>
    </member>
    <member kind="function">
      <type>stp_node_sortfunc</type>
      <name>stp_list_get_sortfunc</name>
      <anchor>a28</anchor>
      <arglist>(const stp_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_item_create</name>
      <anchor>a29</anchor>
      <arglist>(stp_list_t *list, stp_list_item_t *next, const void *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_item_destroy</name>
      <anchor>a30</anchor>
      <arglist>(stp_list_t *list, stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_item_prev</name>
      <anchor>a31</anchor>
      <arglist>(const stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_list_item_t *</type>
      <name>stp_list_item_next</name>
      <anchor>a32</anchor>
      <arglist>(const stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_list_item_get_data</name>
      <anchor>a33</anchor>
      <arglist>(const stp_list_item_t *item)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_list_item_set_data</name>
      <anchor>a34</anchor>
      <arglist>(stp_list_item_t *item, void *data)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-olympus.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-olympus_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">ink_t</class>
    <class kind="struct">ink_list_t</class>
    <class kind="struct">olymp_resolution_t</class>
    <class kind="struct">olymp_resolution_list_t</class>
    <class kind="struct">olymp_pagesize_t</class>
    <class kind="struct">olymp_pagesize_list_t</class>
    <class kind="struct">olymp_printsize_t</class>
    <class kind="struct">olymp_printsize_list_t</class>
    <class kind="struct">laminate_t</class>
    <class kind="struct">laminate_list_t</class>
    <class kind="struct">olympus_privdata_t</class>
    <class kind="struct">olympus_cap_t</class>
    <class kind="struct">float_param_t</class>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_INTERLACE_NONE</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_INTERLACE_LINE</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_INTERLACE_PLANE</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_FEATURE_NONE</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_FEATURE_FULL_WIDTH</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_FEATURE_FULL_HEIGHT</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_FEATURE_BLOCK_ALIGN</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_FEATURE_BORDERLESS</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>OLYMPUS_FEATURE_WHITE_BORDER</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MIN</name>
      <anchor>a9</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>MAX</name>
      <anchor>a10</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>const olympus_cap_t *</type>
      <name>olympus_get_model_capabilities</name>
      <anchor>a97</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const laminate_t *</type>
      <name>olympus_get_laminate_pattern</name>
      <anchor>a98</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p10_printer_init_func</name>
      <anchor>a99</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p10_printer_end_func</name>
      <anchor>a100</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p10_block_init_func</name>
      <anchor>a101</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p200_printer_init_func</name>
      <anchor>a102</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p200_plane_init_func</name>
      <anchor>a103</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p200_printer_end_func</name>
      <anchor>a104</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p300_printer_init_func</name>
      <anchor>a105</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p300_plane_end_func</name>
      <anchor>a106</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p300_block_init_func</name>
      <anchor>a107</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p400_printer_init_func</name>
      <anchor>a108</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p400_plane_init_func</name>
      <anchor>a109</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p400_plane_end_func</name>
      <anchor>a110</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p400_block_init_func</name>
      <anchor>a111</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p440_printer_init_func</name>
      <anchor>a112</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p440_printer_end_func</name>
      <anchor>a113</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p440_block_init_func</name>
      <anchor>a114</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>p440_block_end_func</name>
      <anchor>a115</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>cpx00_printer_init_func</name>
      <anchor>a116</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>cpx00_plane_init_func</name>
      <anchor>a117</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>updp10_printer_init_func</name>
      <anchor>a118</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>updp10_printer_end_func</name>
      <anchor>a119</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>cx400_printer_init_func</name>
      <anchor>a120</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>olympus_printsize</name>
      <anchor>a121</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>olympus_feature</name>
      <anchor>a122</anchor>
      <arglist>(const olympus_cap_t *caps, int feature)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>olympus_list_parameters</name>
      <anchor>a123</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>olympus_parameters</name>
      <anchor>a124</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>olympus_imageable_area</name>
      <anchor>a125</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>olympus_limit</name>
      <anchor>a126</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>olympus_describe_resolution</name>
      <anchor>a127</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>olympus_describe_output</name>
      <anchor>a128</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>unsigned short *</type>
      <name>olympus_get_cached_output</name>
      <anchor>a129</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, unsigned short **cache, int line, int size)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>olympus_do_print</name>
      <anchor>a130</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>olympus_print</name>
      <anchor>a131</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_olympus_module_init</name>
      <anchor>a132</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_olympus_module_exit</name>
      <anchor>a133</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char *</type>
      <name>zero</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>olympus_privdata_t</type>
      <name>privdata</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_t</type>
      <name>cmy_inks</name>
      <anchor>a15</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_list_t</type>
      <name>cmy_ink_list</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_t</type>
      <name>ymc_inks</name>
      <anchor>a17</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_list_t</type>
      <name>ymc_ink_list</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_t</type>
      <name>rgb_inks</name>
      <anchor>a19</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_list_t</type>
      <name>rgb_ink_list</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_t</type>
      <name>bgr_inks</name>
      <anchor>a21</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_list_t</type>
      <name>bgr_ink_list</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_t</type>
      <name>res_320dpi</name>
      <anchor>a23</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_list_t</type>
      <name>res_320dpi_list</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>p10_page</name>
      <anchor>a25</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>p10_page_list</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>p10_printsize</name>
      <anchor>a27</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>p10_printsize_list</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const laminate_t</type>
      <name>p10_laminate</name>
      <anchor>a29</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const laminate_list_t</type>
      <name>p10_laminate_list</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>p200_page</name>
      <anchor>a31</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>p200_page_list</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>p200_printsize</name>
      <anchor>a33</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>p200_printsize_list</name>
      <anchor>a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>p200_adj_any</name>
      <anchor>a35</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_t</type>
      <name>p300_res</name>
      <anchor>a36</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_list_t</type>
      <name>p300_res_list</name>
      <anchor>a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>p300_page</name>
      <anchor>a38</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>p300_page_list</name>
      <anchor>a39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>p300_printsize</name>
      <anchor>a40</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>p300_printsize_list</name>
      <anchor>a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>p300_adj_cyan</name>
      <anchor>a42</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>p300_adj_magenta</name>
      <anchor>a43</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>p300_adj_yellow</name>
      <anchor>a44</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_t</type>
      <name>res_314dpi</name>
      <anchor>a45</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_list_t</type>
      <name>res_314dpi_list</name>
      <anchor>a46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>p400_page</name>
      <anchor>a47</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>p400_page_list</name>
      <anchor>a48</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>p400_printsize</name>
      <anchor>a49</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>p400_printsize_list</name>
      <anchor>a50</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>p400_adj_cyan</name>
      <anchor>a51</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>p400_adj_magenta</name>
      <anchor>a52</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>p400_adj_yellow</name>
      <anchor>a53</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>p440_page</name>
      <anchor>a54</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>p440_page_list</name>
      <anchor>a55</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>p440_printsize</name>
      <anchor>a56</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>p440_printsize_list</name>
      <anchor>a57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>cpx00_page</name>
      <anchor>a58</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>cpx00_page_list</name>
      <anchor>a59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>cpx00_printsize</name>
      <anchor>a60</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>cpx00_printsize_list</name>
      <anchor>a61</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>cpx00_adj_cyan</name>
      <anchor>a62</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>cpx00_adj_magenta</name>
      <anchor>a63</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>cpx00_adj_yellow</name>
      <anchor>a64</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>cp220_page</name>
      <anchor>a65</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>cp220_page_list</name>
      <anchor>a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>cp220_printsize</name>
      <anchor>a67</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>cp220_printsize_list</name>
      <anchor>a68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_t</type>
      <name>updp10_res</name>
      <anchor>a69</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_list_t</type>
      <name>updp10_res_list</name>
      <anchor>a70</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>updp10_page</name>
      <anchor>a71</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>updp10_page_list</name>
      <anchor>a72</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>updp10_printsize</name>
      <anchor>a73</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>updp10_printsize_list</name>
      <anchor>a74</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const laminate_t</type>
      <name>updp10_laminate</name>
      <anchor>a75</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const laminate_list_t</type>
      <name>updp10_laminate_list</name>
      <anchor>a76</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>updp10_adj_cyan</name>
      <anchor>a77</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>updp10_adj_magenta</name>
      <anchor>a78</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>updp10_adj_yellow</name>
      <anchor>a79</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_t</type>
      <name>cx400_res</name>
      <anchor>a80</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_list_t</type>
      <name>cx400_res_list</name>
      <anchor>a81</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_t</type>
      <name>cx400_page</name>
      <anchor>a82</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_pagesize_list_t</type>
      <name>cx400_page_list</name>
      <anchor>a83</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_t</type>
      <name>cx400_printsize</name>
      <anchor>a84</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_printsize_list_t</type>
      <name>cx400_printsize_list</name>
      <anchor>a85</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_t</type>
      <name>all_resolutions</name>
      <anchor>a86</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olymp_resolution_list_t</type>
      <name>all_res_list</name>
      <anchor>a87</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const olympus_cap_t</type>
      <name>olympus_model_capabilities</name>
      <anchor>a88</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a89</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>int</type>
      <name>the_parameter_count</name>
      <anchor>a90</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const float_param_t</type>
      <name>float_parameters</name>
      <anchor>a91</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>float_parameter_count</name>
      <anchor>a92</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_printfuncs_t</type>
      <name>print_olympus_printfuncs</name>
      <anchor>a93</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_family_t</type>
      <name>print_olympus_module_data</name>
      <anchor>a94</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a96</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-papers.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-papers_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_paper_freefunc</name>
      <anchor>a1</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>stpi_paper_namefunc</name>
      <anchor>a2</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>stpi_paper_long_namefunc</name>
      <anchor>a3</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_paper_list_init</name>
      <anchor>a4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_paperlist</name>
      <anchor>a5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_paper_create</name>
      <anchor>a6</anchor>
      <arglist>(stp_papersize_t *p)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_paper_destroy</name>
      <anchor>a7</anchor>
      <arglist>(stp_papersize_t *p)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_known_papersizes</name>
      <anchor>ga8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_name</name>
      <anchor>ga9</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_index</name>
      <anchor>ga10</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>paper_size_mismatch</name>
      <anchor>a11</anchor>
      <arglist>(int l, int w, const stp_papersize_t *val)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_size</name>
      <anchor>ga12</anchor>
      <arglist>(int l, int w)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_default_media_size</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_papersize_t *</type>
      <name>stp_xml_process_paper</name>
      <anchor>a14</anchor>
      <arglist>(stp_mxml_node_t *paper)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stp_xml_process_paperdef</name>
      <anchor>a15</anchor>
      <arglist>(stp_mxml_node_t *paperdef, const char *file)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_paper</name>
      <anchor>ga16</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_list_t *</type>
      <name>paper_list</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-pcl.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-pcl_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <class kind="struct">pcl_privdata_t</class>
    <class kind="struct">pcl_t</class>
    <class kind="struct">margins_t</class>
    <class kind="struct">pcl_cap_t</class>
    <class kind="struct">float_param_t</class>
    <member kind="define">
      <type>#define</type>
      <name>MAX</name>
      <anchor>a0</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_EXECUTIVE</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_LETTER</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_LEGAL</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_TABLOID</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_STATEMENT</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_SUPER_B</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_A5</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_A4</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_A3</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_JIS_B5</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_JIS_B4</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_HAGAKI_CARD</name>
      <anchor>a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_OUFUKU_CARD</name>
      <anchor>a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_A6_CARD</name>
      <anchor>a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_4x6</name>
      <anchor>a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_5x8</name>
      <anchor>a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_3x5</name>
      <anchor>a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_MONARCH_ENV</name>
      <anchor>a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_COMMERCIAL10_ENV</name>
      <anchor>a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_DL_ENV</name>
      <anchor>a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_C5_ENV</name>
      <anchor>a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_C6_ENV</name>
      <anchor>a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_CUSTOM</name>
      <anchor>a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_INVITATION_ENV</name>
      <anchor>a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_JAPANESE_3_ENV</name>
      <anchor>a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_JAPANESE_4_ENV</name>
      <anchor>a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_KAKU_ENV</name>
      <anchor>a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSIZE_HP_CARD</name>
      <anchor>a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>NUM_PRINTER_PAPER_SIZES</name>
      <anchor>a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERTYPE_PLAIN</name>
      <anchor>a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERTYPE_BOND</name>
      <anchor>a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERTYPE_PREMIUM</name>
      <anchor>a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERTYPE_GLOSSY</name>
      <anchor>a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERTYPE_TRANS</name>
      <anchor>a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERTYPE_QPHOTO</name>
      <anchor>a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERTYPE_QTRANS</name>
      <anchor>a36</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>NUM_PRINTER_PAPER_TYPES</name>
      <anchor>a37</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PAPERSOURCE_MOD</name>
      <anchor>a38</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_STANDARD</name>
      <anchor>a39</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_MANUAL</name>
      <anchor>a40</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_ENVELOPE</name>
      <anchor>a41</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_LJ_TRAY2</name>
      <anchor>a42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_LJ_TRAY3</name>
      <anchor>a43</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_LJ_TRAY4</name>
      <anchor>a44</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_LJ_TRAY1</name>
      <anchor>a45</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_340_PCSF</name>
      <anchor>a46</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_340_DCSF</name>
      <anchor>a47</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_DJ_TRAY</name>
      <anchor>a48</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_DJ_TRAY2</name>
      <anchor>a49</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_DJ_OPTIONAL</name>
      <anchor>a50</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PAPERSOURCE_DJ_AUTO</name>
      <anchor>a51</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>NUM_PRINTER_PAPER_SOURCES</name>
      <anchor>a52</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_RES_150_150</name>
      <anchor>a53</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_RES_300_300</name>
      <anchor>a54</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_RES_600_300</name>
      <anchor>a55</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_RES_600_600_MONO</name>
      <anchor>a56</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_RES_600_600</name>
      <anchor>a57</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_RES_1200_600</name>
      <anchor>a58</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_RES_2400_600</name>
      <anchor>a59</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>NUM_RESOLUTIONS</name>
      <anchor>a60</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_COLOR_NONE</name>
      <anchor>a61</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_COLOR_CMY</name>
      <anchor>a62</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_COLOR_CMYK</name>
      <anchor>a63</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_COLOR_CMYK4</name>
      <anchor>a64</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_COLOR_CMYKcm</name>
      <anchor>a65</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_COLOR_CMYK4b</name>
      <anchor>a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_LJ</name>
      <anchor>a67</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_DJ</name>
      <anchor>a68</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_NEW_ERG</name>
      <anchor>a69</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_TIFF</name>
      <anchor>a70</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_MEDIATYPE</name>
      <anchor>a71</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_CUSTOM_SIZE</name>
      <anchor>a72</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_BLANKLINE</name>
      <anchor>a73</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>PCL_PRINTER_DUPLEX</name>
      <anchor>a74</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>NUM_DUPLEX</name>
      <anchor>a75</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a76</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a77</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>pcl_mode0</name>
      <anchor>a124</anchor>
      <arglist>(stp_vars_t *, unsigned char *, int, int)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>pcl_mode2</name>
      <anchor>a125</anchor>
      <arglist>(stp_vars_t *, unsigned char *, int, int)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>pcl_describe_resolution</name>
      <anchor>a126</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>pcl_string_to_val</name>
      <anchor>a127</anchor>
      <arglist>(const char *string, const pcl_t *options, int num_options)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>pcl_val_to_string</name>
      <anchor>a128</anchor>
      <arglist>(int code, const pcl_t *options, int num_options)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>pcl_val_to_text</name>
      <anchor>a129</anchor>
      <arglist>(int code, const pcl_t *options, int num_options)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const pcl_cap_t *</type>
      <name>pcl_get_model_capabilities</name>
      <anchor>a130</anchor>
      <arglist>(int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>pcl_convert_media_size</name>
      <anchor>a131</anchor>
      <arglist>(const char *media_size, int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const int</type>
      <name>pcl_papersize_valid</name>
      <anchor>a132</anchor>
      <arglist>(const stp_papersize_t *pt, int model)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>pcl_list_parameters</name>
      <anchor>a133</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>pcl_parameters</name>
      <anchor>a134</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>internal_imageable_area</name>
      <anchor>a135</anchor>
      <arglist>(const stp_vars_t *v, int use_paper_margins, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>pcl_imageable_area</name>
      <anchor>a136</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>pcl_limit</name>
      <anchor>a137</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>pcl_describe_output</name>
      <anchor>a138</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>pcl_printfunc</name>
      <anchor>a139</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>double</type>
      <name>get_double_param</name>
      <anchor>a140</anchor>
      <arglist>(stp_vars_t *v, const char *param)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>pcl_do_print</name>
      <anchor>a141</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>pcl_print</name>
      <anchor>a142</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_pcl_module_init</name>
      <anchor>a143</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_pcl_module_exit</name>
      <anchor>a144</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_dotsize_t</type>
      <name>single_dotsize</name>
      <anchor>a78</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>photo_dither_shades</name>
      <anchor>a79</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const pcl_t</type>
      <name>pcl_media_sizes</name>
      <anchor>a80</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const pcl_t</type>
      <name>pcl_media_types</name>
      <anchor>a81</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const pcl_t</type>
      <name>pcl_media_sources</name>
      <anchor>a82</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const pcl_t</type>
      <name>pcl_resolutions</name>
      <anchor>a83</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>emptylist</name>
      <anchor>a84</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>standard_papersizes</name>
      <anchor>a85</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>letter_only_papersizes</name>
      <anchor>a86</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj340_papersizes</name>
      <anchor>a87</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj400_papersizes</name>
      <anchor>a88</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj500_papersizes</name>
      <anchor>a89</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj540_papersizes</name>
      <anchor>a90</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj600_papersizes</name>
      <anchor>a91</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj1220_papersizes</name>
      <anchor>a92</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj1100_papersizes</name>
      <anchor>a93</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj1200_papersizes</name>
      <anchor>a94</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj2000_papersizes</name>
      <anchor>a95</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj2500_papersizes</name>
      <anchor>a96</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>ljsmall_papersizes</name>
      <anchor>a97</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>ljbig_papersizes</name>
      <anchor>a98</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>basic_papertypes</name>
      <anchor>a99</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>new_papertypes</name>
      <anchor>a100</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>laserjet_papersources</name>
      <anchor>a101</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj340_papersources</name>
      <anchor>a102</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj_papersources</name>
      <anchor>a103</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>dj2500_papersources</name>
      <anchor>a104</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const short</type>
      <name>standard_papersources</name>
      <anchor>a105</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const pcl_cap_t</type>
      <name>pcl_model_capabilities</name>
      <anchor>a106</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_sat_adjustment</name>
      <anchor>a107</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_lum_adjustment</name>
      <anchor>a108</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char</type>
      <name>standard_hue_adjustment</name>
      <anchor>a109</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a110</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>the_parameter_count</name>
      <anchor>a111</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const float_param_t</type>
      <name>float_parameters</name>
      <anchor>a112</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>float_parameter_count</name>
      <anchor>a113</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const double</type>
      <name>dot_sizes</name>
      <anchor>a114</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const double</type>
      <name>dot_sizes_cret</name>
      <anchor>a115</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_dotsize_t</type>
      <name>variable_dotsizes</name>
      <anchor>a116</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_shade_t</type>
      <name>variable_shades</name>
      <anchor>a117</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_param_string_t</type>
      <name>ink_types</name>
      <anchor>a118</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_param_string_t</type>
      <name>duplex_types</name>
      <anchor>a119</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_printfuncs_t</type>
      <name>print_pcl_printfuncs</name>
      <anchor>a120</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_family_t</type>
      <name>print_pcl_module_data</name>
      <anchor>a121</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a122</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a123</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-ps.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-ps_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_hex</name>
      <anchor>a10</anchor>
      <arglist>(const stp_vars_t *, unsigned short *, int)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_ascii85</name>
      <anchor>a11</anchor>
      <arglist>(const stp_vars_t *, unsigned short *, int, int)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>char *</type>
      <name>ppd_find</name>
      <anchor>a12</anchor>
      <arglist>(const char *, const char *, const char *, int *)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>ps_list_parameters</name>
      <anchor>a13</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_parameters_internal</name>
      <anchor>a14</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_parameters</name>
      <anchor>a15</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_media_size_internal</name>
      <anchor>a16</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_media_size</name>
      <anchor>a17</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_imageable_area_internal</name>
      <anchor>a18</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_imageable_area</name>
      <anchor>a19</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_limit</name>
      <anchor>a20</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_describe_resolution_internal</name>
      <anchor>a21</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>ps_describe_resolution</name>
      <anchor>a22</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>ps_describe_output</name>
      <anchor>a23</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>ps_print_internal</name>
      <anchor>a24</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>ps_print</name>
      <anchor>a25</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_ps_module_init</name>
      <anchor>a26</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_ps_module_exit</name>
      <anchor>a27</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>FILE *</type>
      <name>ps_ppd</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const char *</type>
      <name>ps_ppd_file</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>the_parameter_count</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_printfuncs_t</type>
      <name>print_ps_printfuncs</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_family_t</type>
      <name>print_ps_module_data</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-raw.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-raw_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">ink_t</class>
    <class kind="struct">raw_printer</class>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_version</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>stp_module_data</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>raw_printer</type>
      <name>raw_printer_t</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_list_t</type>
      <name>raw_list_parameters</name>
      <anchor>a12</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>raw_parameters</name>
      <anchor>a13</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>raw_imageable_area</name>
      <anchor>a14</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>raw_limit</name>
      <anchor>a15</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>raw_describe_resolution</name>
      <anchor>a16</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>raw_describe_output</name>
      <anchor>a17</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>raw_print</name>
      <anchor>a18</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_raw_module_init</name>
      <anchor>a19</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>print_raw_module_exit</name>
      <anchor>a20</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const raw_printer_t</type>
      <name>raw_model_capabilities</name>
      <anchor>a3</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const ink_t</type>
      <name>inks</name>
      <anchor>a4</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>ink_count</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_parameter_t</type>
      <name>the_parameters</name>
      <anchor>a6</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const int</type>
      <name>the_parameter_count</name>
      <anchor>a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>const stp_printfuncs_t</type>
      <name>print_raw_printfuncs</name>
      <anchor>a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_family_t</type>
      <name>print_raw_module_data</name>
      <anchor>a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_version_t</type>
      <name>stp_module_version</name>
      <anchor>a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_t</type>
      <name>stp_module_data</name>
      <anchor>a11</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-util.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-util_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="generic-options_8h" name="generic-options.h" local="yes">generic-options.h</includes>
    <class kind="struct">debug_msgbuf_t</class>
    <member kind="define">
      <type>#define</type>
      <name>FMIN</name>
      <anchor>a0</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STPI_VASPRINTF</name>
      <anchor>a1</anchor>
      <arglist>(result, bytes, format)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>BYTE</name>
      <anchor>a2</anchor>
      <arglist>(expr, byteno)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_zprintf</name>
      <anchor>a7</anchor>
      <arglist>(const stp_vars_t *v, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_asprintf</name>
      <anchor>a8</anchor>
      <arglist>(char **strp, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_catprintf</name>
      <anchor>a9</anchor>
      <arglist>(char **strp, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_zfwrite</name>
      <anchor>ga10</anchor>
      <arglist>(const char *buf, size_t bytes, size_t nitems, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_putc</name>
      <anchor>ga11</anchor>
      <arglist>(int ch, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put16_le</name>
      <anchor>ga12</anchor>
      <arglist>(unsigned short sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put16_be</name>
      <anchor>ga13</anchor>
      <arglist>(unsigned short sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put32_le</name>
      <anchor>ga14</anchor>
      <arglist>(unsigned int in, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put32_be</name>
      <anchor>ga15</anchor>
      <arglist>(unsigned int in, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_puts</name>
      <anchor>ga16</anchor>
      <arglist>(const char *s, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_send_command</name>
      <anchor>ga17</anchor>
      <arglist>(const stp_vars_t *v, const char *command, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_eprintf</name>
      <anchor>a18</anchor>
      <arglist>(const stp_vars_t *v, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_erputc</name>
      <anchor>ga19</anchor>
      <arglist>(int ch)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_erprintf</name>
      <anchor>a20</anchor>
      <arglist>(const char *format,...)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_init_debug</name>
      <anchor>a21</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>unsigned long</type>
      <name>stp_get_debug_level</name>
      <anchor>ga22</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dprintf</name>
      <anchor>a23</anchor>
      <arglist>(unsigned long level, const stp_vars_t *v, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_deprintf</name>
      <anchor>a24</anchor>
      <arglist>(unsigned long level, const char *format,...)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>fill_buffer_writefunc</name>
      <anchor>a25</anchor>
      <arglist>(void *priv, const char *buffer, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_init_debug_messages</name>
      <anchor>ga26</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_flush_debug_messages</name>
      <anchor>ga27</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_malloc</name>
      <anchor>ga28</anchor>
      <arglist>(size_t size)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_zalloc</name>
      <anchor>ga29</anchor>
      <arglist>(size_t size)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_realloc</name>
      <anchor>ga30</anchor>
      <arglist>(void *ptr, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_free</name>
      <anchor>ga31</anchor>
      <arglist>(void *ptr)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_init</name>
      <anchor>ga32</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_strlen</name>
      <anchor>ga33</anchor>
      <arglist>(const char *s)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_strndup</name>
      <anchor>ga34</anchor>
      <arglist>(const char *s, int n)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_strdup</name>
      <anchor>ga35</anchor>
      <arglist>(const char *s)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_set_output_codeset</name>
      <anchor>ga36</anchor>
      <arglist>(const char *codeset)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_read_and_compose_curves</name>
      <anchor>ga37</anchor>
      <arglist>(const char *s1, const char *s2, stp_curve_compose_t comp, size_t piecewise_point_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_merge_printvars</name>
      <anchor>ga38</anchor>
      <arglist>(stp_vars_t *user, const stp_vars_t *print)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_get_parameter_list</name>
      <anchor>ga39</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_abort</name>
      <anchor>ga40</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>unsigned long</type>
      <name>stpi_debug_level</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *(*</type>
      <name>stp_malloc_func</name>
      <anchor>a4</anchor>
      <arglist>)(size_t size)=malloc</arglist>
    </member>
    <member kind="variable">
      <type>void *(*</type>
      <name>stpi_realloc_func</name>
      <anchor>a5</anchor>
      <arglist>)(void *ptr, size_t size)=realloc</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>stpi_free_func</name>
      <anchor>a6</anchor>
      <arglist>)(void *ptr)=free</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-vars.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-vars_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <includes id="generic-options_8h" name="generic-options.h" local="yes">generic-options.h</includes>
    <class kind="struct">value_t</class>
    <class kind="struct">stp_compdata</class>
    <class kind="struct">stp_vars</class>
    <member kind="define">
      <type>#define</type>
      <name>DEF_STRING_FUNCS</name>
      <anchor>a0</anchor>
      <arglist>(s, pre)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEF_FUNCS</name>
      <anchor>a1</anchor>
      <arglist>(s, t, pre)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHECK_FUNCTION</name>
      <anchor>a2</anchor>
      <arglist>(type, index)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GET_PARAMETER_ACTIVE_FUNCTION</name>
      <anchor>a3</anchor>
      <arglist>(type, index)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>SET_PARAMETER_ACTIVE_FUNCTION</name>
      <anchor>a4</anchor>
      <arglist>(type, index)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>null_vars</name>
      <anchor>a7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_vars</name>
      <anchor>a8</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>value_namefunc</name>
      <anchor>a9</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>value_freefunc</name>
      <anchor>a10</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_t *</type>
      <name>create_vars_list</name>
      <anchor>a11</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>copy_to_raw</name>
      <anchor>a12</anchor>
      <arglist>(stp_raw_t *raw, const void *data, size_t bytes)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>value_t *</type>
      <name>value_copy</name>
      <anchor>a13</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_t *</type>
      <name>copy_value_list</name>
      <anchor>a14</anchor>
      <arglist>(const stp_list_t *src)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>compdata_namefunc</name>
      <anchor>a15</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>compdata_freefunc</name>
      <anchor>a16</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void *</type>
      <name>compdata_copyfunc</name>
      <anchor>a17</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_allocate_component_data</name>
      <anchor>ga18</anchor>
      <arglist>(stp_vars_t *v, const char *name, stp_copy_data_func_t copyfunc, stp_free_data_func_t freefunc, void *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_destroy_component_data</name>
      <anchor>ga19</anchor>
      <arglist>(stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_get_component_data</name>
      <anchor>ga20</anchor>
      <arglist>(const stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_t *</type>
      <name>create_compdata_list</name>
      <anchor>a21</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_list_t *</type>
      <name>copy_compdata_list</name>
      <anchor>a22</anchor>
      <arglist>(const stp_list_t *src)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>initialize_standard_vars</name>
      <anchor>a23</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_vars_t *</type>
      <name>stp_default_settings</name>
      <anchor>ga24</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_vars_t *</type>
      <name>stp_vars_create</name>
      <anchor>ga25</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_vars_destroy</name>
      <anchor>ga26</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_verified</name>
      <anchor>ga27</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_verified</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_default_raw_parameter</name>
      <anchor>a29</anchor>
      <arglist>(stp_list_t *list, const char *parameter, const char *value, size_t bytes, int typ)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_raw_parameter</name>
      <anchor>a30</anchor>
      <arglist>(stp_list_t *list, const char *parameter, const char *value, size_t bytes, int typ)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter_n</name>
      <anchor>ga31</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter</name>
      <anchor>ga32</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_string_parameter_n</name>
      <anchor>ga33</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_string_parameter</name>
      <anchor>ga34</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_string_parameter</name>
      <anchor>ga35</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_string_parameter</name>
      <anchor>ga36</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_raw_parameter</name>
      <anchor>ga37</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const void *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_raw_parameter</name>
      <anchor>ga38</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const void *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_raw_parameter</name>
      <anchor>ga39</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_raw_t *</type>
      <name>stp_get_raw_parameter</name>
      <anchor>ga40</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter</name>
      <anchor>ga41</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter_n</name>
      <anchor>ga42</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t byte_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_file_parameter</name>
      <anchor>ga43</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_file_parameter_n</name>
      <anchor>ga44</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t byte_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_file_parameter</name>
      <anchor>ga45</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_file_parameter</name>
      <anchor>ga46</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_curve_parameter</name>
      <anchor>ga47</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_curve_parameter</name>
      <anchor>ga48</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_curve_parameter</name>
      <anchor>ga49</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_curve_t *</type>
      <name>stp_get_curve_parameter</name>
      <anchor>ga50</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_array_parameter</name>
      <anchor>ga51</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_array_parameter</name>
      <anchor>ga52</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_array_parameter</name>
      <anchor>ga53</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_array_t *</type>
      <name>stp_get_array_parameter</name>
      <anchor>ga54</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_int_parameter</name>
      <anchor>ga55</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int ival)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_int_parameter</name>
      <anchor>ga56</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int ival)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_int_parameter</name>
      <anchor>ga57</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_int_parameter</name>
      <anchor>ga58</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_boolean_parameter</name>
      <anchor>ga59</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int ival)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_boolean_parameter</name>
      <anchor>ga60</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int ival)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_boolean_parameter</name>
      <anchor>ga61</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_boolean_parameter</name>
      <anchor>ga62</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_dimension_parameter</name>
      <anchor>ga63</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int ival)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_dimension_parameter</name>
      <anchor>ga64</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int ival)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_dimension_parameter</name>
      <anchor>ga65</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_dimension_parameter</name>
      <anchor>ga66</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_float_parameter</name>
      <anchor>ga67</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double dval)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_float_parameter</name>
      <anchor>ga68</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double dval)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_float_parameter</name>
      <anchor>ga69</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_get_float_parameter</name>
      <anchor>ga70</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_scale_float_parameter</name>
      <anchor>ga71</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double scale)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>check_parameter_generic</name>
      <anchor>a72</anchor>
      <arglist>(const stp_vars_t *v, stp_parameter_type_t p_type, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_parameter_activity_t</type>
      <name>get_parameter_active_generic</name>
      <anchor>a73</anchor>
      <arglist>(const stp_vars_t *v, stp_parameter_type_t p_type, const char *parameter)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_parameter_active_generic</name>
      <anchor>a74</anchor>
      <arglist>(const stp_vars_t *v, stp_parameter_type_t p_type, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_fill_parameter_settings</name>
      <anchor>ga75</anchor>
      <arglist>(stp_parameter_t *desc, const stp_parameter_t *param)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_vars_copy</name>
      <anchor>ga76</anchor>
      <arglist>(stp_vars_t *vd, const stp_vars_t *vs)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_prune_inactive_options</name>
      <anchor>ga77</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>stp_vars_t *</type>
      <name>stp_vars_create_copy</name>
      <anchor>ga78</anchor>
      <arglist>(const stp_vars_t *vs)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>param_namefunc</name>
      <anchor>a79</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>param_longnamefunc</name>
      <anchor>a80</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_parameter_list_create</name>
      <anchor>ga81</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_add_param</name>
      <anchor>ga82</anchor>
      <arglist>(stp_parameter_list_t list, const stp_parameter_t *item)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_describe_parameter</name>
      <anchor>ga83</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_description_destroy</name>
      <anchor>ga84</anchor>
      <arglist>(stp_parameter_t *desc)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_find_in_settings</name>
      <anchor>ga85</anchor>
      <arglist>(const stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_parameter_list_count</name>
      <anchor>ga86</anchor>
      <arglist>(stp_const_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_find</name>
      <anchor>ga87</anchor>
      <arglist>(stp_const_parameter_list_t list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_list_param</name>
      <anchor>ga88</anchor>
      <arglist>(stp_const_parameter_list_t list, size_t item)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_destroy</name>
      <anchor>ga89</anchor>
      <arglist>(stp_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_parameter_list_copy</name>
      <anchor>ga90</anchor>
      <arglist>(stp_const_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_append</name>
      <anchor>ga91</anchor>
      <arglist>(stp_parameter_list_t list, stp_const_parameter_list_t append)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>int</type>
      <name>standard_vars_initialized</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_vars_t</type>
      <name>default_vars</name>
      <anchor>a6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-version.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-version_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <member kind="function">
      <type>const char *</type>
      <name>stp_check_version</name>
      <anchor>ga6</anchor>
      <arglist>(unsigned int required_major, unsigned int required_minor, unsigned int required_micro)</arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_major_version</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_minor_version</name>
      <anchor>ga1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_micro_version</name>
      <anchor>ga2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_current_interface</name>
      <anchor>ga3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_binary_age</name>
      <anchor>ga4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_interface_age</name>
      <anchor>ga5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>print-weave.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>print-weave_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stpi_softweave</class>
    <class kind="struct">raw</class>
    <class kind="struct">cooked</class>
    <member kind="define">
      <type>#define</type>
      <name>ASSERTIONS</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>assert</name>
      <anchor>a1</anchor>
      <arglist>(x, v)</arglist>
    </member>
    <member kind="typedef">
      <type>stpi_softweave</type>
      <name>stpi_softweave_t</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>raw</type>
      <name>raw_t</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>cooked</type>
      <name>cooked_t</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>gcd</name>
      <anchor>a5</anchor>
      <arglist>(int x, int y)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>initialize_raw_weave</name>
      <anchor>a6</anchor>
      <arglist>(raw_t *w, int separation, int jets, int oversample, stp_weave_strategy_t strat, stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>calculate_raw_pass_parameters</name>
      <anchor>a7</anchor>
      <arglist>(raw_t *w, int pass, int *startrow, int *subpass)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>calculate_raw_row_parameters</name>
      <anchor>a8</anchor>
      <arglist>(raw_t *w, int row, int subpass, int *pass, int *jet, int *startrow)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>sort_by_start_row</name>
      <anchor>a9</anchor>
      <arglist>(int *map, int *startrows, int count)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>calculate_stagger</name>
      <anchor>a10</anchor>
      <arglist>(raw_t *w, int *map, int *startrows_stagger, int count)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>invert_map</name>
      <anchor>a11</anchor>
      <arglist>(int *map, int *stagger, int count, int oldfirstpass, int newfirstpass)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>make_passmap</name>
      <anchor>a12</anchor>
      <arglist>(raw_t *w, int **map, int **starts, int first_pass_number, int first_pass_to_map, int first_pass_after_map, int first_pass_to_stagger, int first_pass_after_stagger, int first_row_of_maximal_pass, int separations_to_distribute)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>calculate_pass_map</name>
      <anchor>a13</anchor>
      <arglist>(stp_vars_t *v, cooked_t *w, int pageheight, int firstrow, int lastrow)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void *</type>
      <name>initialize_weave_params</name>
      <anchor>a14</anchor>
      <arglist>(int separation, int jets, int oversample, int firstrow, int lastrow, int pageheight, stp_weave_strategy_t strategy, stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_destroy_weave_params</name>
      <anchor>a15</anchor>
      <arglist>(void *vw)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_calculate_row_parameters</name>
      <anchor>a16</anchor>
      <arglist>(void *vw, int row, int subpass, int *pass, int *jetnum, int *startingrow, int *ophantomrows, int *ojetsused)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_lineoff_t *</type>
      <name>allocate_lineoff</name>
      <anchor>a17</anchor>
      <arglist>(int count, int ncolors)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_lineactive_t *</type>
      <name>allocate_lineactive</name>
      <anchor>a18</anchor>
      <arglist>(int count, int ncolors)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_linecount_t *</type>
      <name>allocate_linecount</name>
      <anchor>a19</anchor>
      <arglist>(int count, int ncolors)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_linebounds_t *</type>
      <name>allocate_linebounds</name>
      <anchor>a20</anchor>
      <arglist>(int count, int ncolors)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_linebufs_t *</type>
      <name>allocate_linebuf</name>
      <anchor>a21</anchor>
      <arglist>(int count, int ncolors)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_destroy_weave</name>
      <anchor>a22</anchor>
      <arglist>(void *vsw)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_initialize_weave</name>
      <anchor>a23</anchor>
      <arglist>(stp_vars_t *v, int jets, int sep, int osample, int v_subpasses, int v_subsample, int ncolors, int bitwidth, int linewidth, int line_count, int first_line, int page_height, const int *head_offset, stp_weave_strategy_t weave_strategy, stp_flushfunc flushfunc, stp_fillfunc fillfunc, stp_packfunc pack, stp_compute_linewidth_func compute_linewidth)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>weave_parameters_by_row</name>
      <anchor>a24</anchor>
      <arglist>(const stp_vars_t *v, const stpi_softweave_t *sw, int row, int vertical_subpass, stp_weave_t *w)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_weave_parameters_by_row</name>
      <anchor>a25</anchor>
      <arglist>(const stp_vars_t *v, int row, int vertical_subpass, stp_weave_t *w)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_lineoff_t *</type>
      <name>stpi_get_lineoffsets</name>
      <anchor>a26</anchor>
      <arglist>(const stp_vars_t *v, const stpi_softweave_t *sw, int row, int subpass, int offset)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_lineactive_t *</type>
      <name>stpi_get_lineactive</name>
      <anchor>a27</anchor>
      <arglist>(const stp_vars_t *v, const stpi_softweave_t *sw, int row, int subpass, int offset)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_linecount_t *</type>
      <name>stpi_get_linecount</name>
      <anchor>a28</anchor>
      <arglist>(const stp_vars_t *v, const stpi_softweave_t *sw, int row, int subpass, int offset)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_linebufs_t *</type>
      <name>stpi_get_linebases</name>
      <anchor>a29</anchor>
      <arglist>(const stp_vars_t *v, const stpi_softweave_t *sw, int row, int subpass, int offset)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_linebounds_t *</type>
      <name>stpi_get_linebounds</name>
      <anchor>a30</anchor>
      <arglist>(const stp_vars_t *v, const stpi_softweave_t *sw, int row, int subpass, int offset)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_pass_t *</type>
      <name>stpi_get_pass_by_row</name>
      <anchor>a31</anchor>
      <arglist>(stp_vars_t *v, const stpi_softweave_t *sw, int row, int subpass, int offset)</arglist>
    </member>
    <member kind="function">
      <type>stp_lineoff_t *</type>
      <name>stp_get_lineoffsets_by_pass</name>
      <anchor>a32</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>stp_lineactive_t *</type>
      <name>stp_get_lineactive_by_pass</name>
      <anchor>a33</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>stp_linecount_t *</type>
      <name>stp_get_linecount_by_pass</name>
      <anchor>a34</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>const stp_linebufs_t *</type>
      <name>stp_get_linebases_by_pass</name>
      <anchor>a35</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function">
      <type>stp_pass_t *</type>
      <name>stp_get_pass_by_pass</name>
      <anchor>a36</anchor>
      <arglist>(const stp_vars_t *v, int pass)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_linebases</name>
      <anchor>a37</anchor>
      <arglist>(stp_vars_t *v, const stpi_softweave_t *sw, int row, int cpass, int head_offset, int color)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_fill_tiff</name>
      <anchor>a38</anchor>
      <arglist>(stp_vars_t *v, int row, int subpass, int width, int missingstartrows, int color)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_fill_uncompressed</name>
      <anchor>a39</anchor>
      <arglist>(stp_vars_t *v, int row, int subpass, int width, int missingstartrows, int color)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_compute_tiff_linewidth</name>
      <anchor>a40</anchor>
      <arglist>(stp_vars_t *v, int n)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_compute_uncompressed_linewidth</name>
      <anchor>a41</anchor>
      <arglist>(stp_vars_t *v, int n)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>initialize_row</name>
      <anchor>a42</anchor>
      <arglist>(stp_vars_t *v, stpi_softweave_t *sw, int row, int width, unsigned char *const cols[])</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>add_to_row</name>
      <anchor>a43</anchor>
      <arglist>(stp_vars_t *v, stpi_softweave_t *sw, int row, unsigned char *buf, size_t nbytes, int color, int setactive, int h_pass)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_flush_passes</name>
      <anchor>a44</anchor>
      <arglist>(stp_vars_t *v, int flushall)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_flush_all</name>
      <anchor>a45</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>finalize_row</name>
      <anchor>a46</anchor>
      <arglist>(stp_vars_t *v, int row)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_write_weave</name>
      <anchor>a47</anchor>
      <arglist>(stp_vars_t *v, unsigned char *const cols[])</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>printers.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>printers_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stp_printer</class>
    <class kind="struct">errbuf_t</class>
    <member kind="define">
      <type>#define</type>
      <name>FMIN</name>
      <anchor>a0</anchor>
      <arglist>(a, b)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHECK_INT_RANGE</name>
      <anchor>a1</anchor>
      <arglist>(v, component, min, max)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>CHECK_INT_RANGE_INTERNAL</name>
      <anchor>a2</anchor>
      <arglist>(v, component, min, max)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_printer_freefunc</name>
      <anchor>a4</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>stpi_printer_namefunc</name>
      <anchor>a5</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>stpi_printer_long_namefunc</name>
      <anchor>a6</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_init_printer_list</name>
      <anchor>a7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_printer_model_count</name>
      <anchor>ga8</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>null_printer</name>
      <anchor>a9</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_printer</name>
      <anchor>a10</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_index</name>
      <anchor>ga11</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_driver</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_long_name</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_family</name>
      <anchor>ga14</anchor>
      <arglist>(const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_manufacturer</name>
      <anchor>ga15</anchor>
      <arglist>(const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_printer_get_model</name>
      <anchor>ga16</anchor>
      <arglist>(const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const stp_printfuncs_t *</type>
      <name>stpi_get_printfuncs</name>
      <anchor>a17</anchor>
      <arglist>(const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function">
      <type>const stp_vars_t *</type>
      <name>stp_printer_get_defaults</name>
      <anchor>ga18</anchor>
      <arglist>(const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_long_name</name>
      <anchor>ga19</anchor>
      <arglist>(const char *long_name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_driver</name>
      <anchor>ga20</anchor>
      <arglist>(const char *driver)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_printer_index_by_driver</name>
      <anchor>ga21</anchor>
      <arglist>(const char *driver)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer</name>
      <anchor>ga22</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_model_id</name>
      <anchor>ga23</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_printer_list_parameters</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_printer_describe_parameter</name>
      <anchor>ga25</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>set_printer_defaults</name>
      <anchor>a26</anchor>
      <arglist>(stp_vars_t *v, int core_only)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_printer_defaults</name>
      <anchor>ga27</anchor>
      <arglist>(stp_vars_t *v, const stp_printer_t *printer)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_initialize_printer_defaults</name>
      <anchor>ga28</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_media_size</name>
      <anchor>ga29</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_imageable_area</name>
      <anchor>ga30</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_size_limit</name>
      <anchor>ga31</anchor>
      <arglist>(const stp_vars_t *v, int *max_width, int *max_height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_describe_resolution</name>
      <anchor>ga32</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_describe_output</name>
      <anchor>ga33</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_verify</name>
      <anchor>ga34</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_print</name>
      <anchor>ga35</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_start_job</name>
      <anchor>ga36</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_end_job</name>
      <anchor>ga37</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_string_param</name>
      <anchor>a38</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_t *desc, int quiet)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_double_param</name>
      <anchor>a39</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_t *desc, int quiet)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_int_param</name>
      <anchor>a40</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_t *desc, int quiet)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_dimension_param</name>
      <anchor>a41</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_t *desc, int quiet)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>verify_curve_param</name>
      <anchor>a42</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_t *desc, int quiet)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_verify_t</type>
      <name>stp_verify_parameter</name>
      <anchor>ga43</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, int quiet)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>fill_buffer_writefunc</name>
      <anchor>a44</anchor>
      <arglist>(void *priv, const char *buffer, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_verify_printer_params</name>
      <anchor>ga45</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_family_register</name>
      <anchor>ga46</anchor>
      <arglist>(stp_list_t *family)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_family_unregister</name>
      <anchor>ga47</anchor>
      <arglist>(stp_list_t *family)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>stp_printer_t *</type>
      <name>stp_printer_create_from_xmltree</name>
      <anchor>a48</anchor>
      <arglist>(stp_mxml_node_t *printer, const char *family, const stp_printfuncs_t *printfuncs)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_xml_process_family</name>
      <anchor>a49</anchor>
      <arglist>(stp_mxml_node_t *family)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>int</type>
      <name>stpi_xml_process_printdef</name>
      <anchor>a50</anchor>
      <arglist>(stp_mxml_node_t *printdef, const char *file)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_printer</name>
      <anchor>ga51</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_list_t *</type>
      <name>printer_list</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sequence.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>sequence_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="sequence_8h" name="sequence.h" local="no">gimp-print/sequence.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stp_sequence</class>
    <member kind="define">
      <type>#define</type>
      <name>DEFINE_DATA_SETTER</name>
      <anchor>a0</anchor>
      <arglist>(t, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>DEFINE_DATA_ACCESSOR</name>
      <anchor>a1</anchor>
      <arglist>(t, lb, ub, name)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>HUGE_VALF</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>check_sequence</name>
      <anchor>a3</anchor>
      <arglist>(const stp_sequence_t *v)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>sequence_ctor</name>
      <anchor>a4</anchor>
      <arglist>(stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create</name>
      <anchor>ga5</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>invalidate_auxilliary_data</name>
      <anchor>a6</anchor>
      <arglist>(stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>sequence_dtor</name>
      <anchor>a7</anchor>
      <arglist>(stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_destroy</name>
      <anchor>ga8</anchor>
      <arglist>(stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_copy</name>
      <anchor>ga9</anchor>
      <arglist>(stp_sequence_t *dest, const stp_sequence_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create_copy</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_bounds</name>
      <anchor>ga11</anchor>
      <arglist>(stp_sequence_t *sequence, double low, double high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_bounds</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_sequence_t *sequence, double *low, double *high)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>scan_sequence_range</name>
      <anchor>a13</anchor>
      <arglist>(stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_range</name>
      <anchor>ga14</anchor>
      <arglist>(const stp_sequence_t *sequence, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_size</name>
      <anchor>ga15</anchor>
      <arglist>(stp_sequence_t *sequence, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_sequence_get_size</name>
      <anchor>ga16</anchor>
      <arglist>(const stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_data</name>
      <anchor>ga17</anchor>
      <arglist>(stp_sequence_t *sequence, size_t size, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_subrange</name>
      <anchor>ga18</anchor>
      <arglist>(stp_sequence_t *sequence, size_t where, size_t size, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_data</name>
      <anchor>ga19</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *size, const double **data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_point</name>
      <anchor>ga20</anchor>
      <arglist>(stp_sequence_t *sequence, size_t where, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_get_point</name>
      <anchor>ga21</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t where, double *data)</arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create_from_xmltree</name>
      <anchor>a22</anchor>
      <arglist>(stp_mxml_node_t *da)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmltree_create_from_sequence</name>
      <anchor>a23</anchor>
      <arglist>(const stp_sequence_t *seq)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>string-list.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>string-list_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <member kind="function" static="yes">
      <type>void</type>
      <name>free_list_element</name>
      <anchor>a0</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>namefunc</name>
      <anchor>a1</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void *</type>
      <name>copyfunc</name>
      <anchor>a2</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>long_namefunc</name>
      <anchor>a3</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_string_list_t *</type>
      <name>stp_string_list_create</name>
      <anchor>a4</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_string_list_destroy</name>
      <anchor>a5</anchor>
      <arglist>(stp_string_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_param_string_t *</type>
      <name>stp_string_list_param</name>
      <anchor>a6</anchor>
      <arglist>(const stp_string_list_t *list, size_t element)</arglist>
    </member>
    <member kind="function">
      <type>stp_param_string_t *</type>
      <name>stp_string_list_find</name>
      <anchor>a7</anchor>
      <arglist>(const stp_string_list_t *list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_string_list_count</name>
      <anchor>a8</anchor>
      <arglist>(const stp_string_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_string_list_t *</type>
      <name>stp_string_list_create_copy</name>
      <anchor>a9</anchor>
      <arglist>(const stp_string_list_t *list)</arglist>
    </member>
    <member kind="function">
      <type>stp_string_list_t *</type>
      <name>stp_string_list_create_from_params</name>
      <anchor>a10</anchor>
      <arglist>(const stp_param_string_t *list, size_t count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_string_list_add_string</name>
      <anchor>a11</anchor>
      <arglist>(stp_string_list_t *list, const char *name, const char *text)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_string_list_remove_string</name>
      <anchor>a12</anchor>
      <arglist>(stp_string_list_t *list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_string_list_is_present</name>
      <anchor>a13</anchor>
      <arglist>(const stp_string_list_t *list, const char *value)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>xml.c</name>
    <path>/home/rlk/sandbox/print-4.3/src/main/</path>
    <filename>xml_8c</filename>
    <includes id="gimp-print_8h" name="gimp-print.h" local="no">gimp-print/gimp-print.h</includes>
    <includes id="gimp-print-internal_8h" name="gimp-print-internal.h" local="yes">gimp-print-internal.h</includes>
    <includes id="gimp-print-intl-internal_8h" name="gimp-print-intl-internal.h" local="no">gimp-print/gimp-print-intl-internal.h</includes>
    <class kind="struct">stpi_xml_parse_registry</class>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>xml_registry_namefunc</name>
      <anchor>a6</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>xml_registry_freefunc</name>
      <anchor>a7</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>const char *</type>
      <name>xml_preload_namefunc</name>
      <anchor>a8</anchor>
      <arglist>(const void *item)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>xml_preload_freefunc</name>
      <anchor>a9</anchor>
      <arglist>(void *item)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_register_xml_parser</name>
      <anchor>a10</anchor>
      <arglist>(const char *name, stp_xml_parse_func parse_func)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unregister_xml_parser</name>
      <anchor>a11</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_register_xml_preload</name>
      <anchor>a12</anchor>
      <arglist>(const char *filename)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_unregister_xml_preload</name>
      <anchor>a13</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_xml_process_gimpprint</name>
      <anchor>a14</anchor>
      <arglist>(stp_mxml_node_t *gimpprint, const char *file)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_preinit</name>
      <anchor>a15</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_init</name>
      <anchor>a16</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_exit</name>
      <anchor>a17</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_xml_parse_file_named</name>
      <anchor>a18</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_xml_init_defaults</name>
      <anchor>a19</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_xml_parse_file</name>
      <anchor>a20</anchor>
      <arglist>(const char *file)</arglist>
    </member>
    <member kind="function">
      <type>long</type>
      <name>stp_xmlstrtol</name>
      <anchor>a21</anchor>
      <arglist>(const char *textval)</arglist>
    </member>
    <member kind="function">
      <type>unsigned long</type>
      <name>stp_xmlstrtoul</name>
      <anchor>a22</anchor>
      <arglist>(const char *textval)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_xmlstrtod</name>
      <anchor>a23</anchor>
      <arglist>(const char *textval)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xml_get_node</name>
      <anchor>a24</anchor>
      <arglist>(stp_mxml_node_t *xmlroot,...)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>void</type>
      <name>stpi_xml_process_node</name>
      <anchor>a25</anchor>
      <arglist>(stp_mxml_node_t *node, const char *file)</arglist>
    </member>
    <member kind="function">
      <type>stp_mxml_node_t *</type>
      <name>stp_xmldoc_create_generic</name>
      <anchor>a26</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_list_t *</type>
      <name>stpi_xml_registry</name>
      <anchor>a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>stp_list_t *</type>
      <name>stpi_xml_preloads</name>
      <anchor>a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>char *</type>
      <name>saved_lc_collate</name>
      <anchor>a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>char *</type>
      <name>saved_lc_ctype</name>
      <anchor>a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>char *</type>
      <name>saved_lc_numeric</name>
      <anchor>a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>int</type>
      <name>xml_is_initialised</name>
      <anchor>a5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_caps</name>
    <filename>structcanon__caps.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>model</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>model_id</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_width</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_height</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>base_res</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_xdpi</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_ydpi</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_quality</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_left</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_right</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_top</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_bottom</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>inks</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>slots</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>features</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dummy</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_dot_size_t</type>
      <name>dot_sizes</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_densities_t</type>
      <name>densities</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_inklist_t *</type>
      <name>inxs</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>inxs_cnt</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>lum_adjustment</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>hue_adjustment</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>sat_adjustment</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_densities</name>
    <filename>structcanon__densities.html</filename>
    <member kind="variable">
      <type>double</type>
      <name>d_r11</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>d_r22</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>d_r33</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>d_r43</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>d_r44</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>d_r55</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_dot_sizes</name>
    <filename>structcanon__dot__sizes.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>dot_r11</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dot_r22</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dot_r33</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dot_r43</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dot_r44</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dot_r55</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_init_t</name>
    <filename>structcanon__init__t.html</filename>
    <member kind="variable">
      <type>const canon_cap_t *</type>
      <name>caps</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>printing_color</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_t *</type>
      <name>pt</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>print_head</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>colormode</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>source_str</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>xdpi</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ydpi</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_width</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_height</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>top</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>left</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bits</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_privdata_t</name>
    <filename>structcanon__privdata__t.html</filename>
    <member kind="variable">
      <type>const canon_cap_t *</type>
      <name>caps</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>cols</name>
      <anchor>o1</anchor>
      <arglist>[7]</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>delay</name>
      <anchor>o2</anchor>
      <arglist>[7]</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>delay_max</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>buf_length</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>out_width</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>left</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>emptylines</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bits</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ydpi</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_res_t</name>
    <filename>structcanon__res__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>x</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name_dmt</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text_dmt</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_variable_ink</name>
    <filename>structcanon__variable__ink.html</filename>
    <member kind="variable">
      <type>double</type>
      <name>density</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_shade_t *</type>
      <name>shades</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>numshades</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_variable_inklist</name>
    <filename>structcanon__variable__inklist.html</filename>
    <member kind="variable">
      <type>const int</type>
      <name>bits</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>colors</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_inkset_t *</type>
      <name>r11</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_inkset_t *</type>
      <name>r22</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_inkset_t *</type>
      <name>r33</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_inkset_t *</type>
      <name>r43</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_inkset_t *</type>
      <name>r44</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_inkset_t *</type>
      <name>r55</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>canon_variable_inkset</name>
    <filename>structcanon__variable__inkset.html</filename>
    <member kind="variable">
      <type>const canon_variable_ink_t *</type>
      <name>c</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_ink_t *</type>
      <name>m</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_ink_t *</type>
      <name>y</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const canon_variable_ink_t *</type>
      <name>k</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>channel_count_t</name>
    <filename>structchannel__count__t.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>count</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>channel_depth_t</name>
    <filename>structchannel__depth__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>bits</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>channel_param_t</name>
    <filename>structchannel__param__t.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>channel_id</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>gamma_name</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>curve_name</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>rgb_gamma_name</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>rgb_curve_name</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>channel_set_t</name>
    <filename>structchannel__set__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const ink_channel_t *const *</type>
      <name>channels</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>channel_count</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>color_correction_t</name>
    <filename>structcolor__correction__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>color_correction_enum_t</type>
      <name>correction</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>correct_hsl</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>color_description_t</name>
    <filename>structcolor__description__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>input</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>output</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>color_id_t</type>
      <name>color_id</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>color_model_t</type>
      <name>color_model</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>channels</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>channel_count</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>color_correction_enum_t</type>
      <name>default_correction</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_convert_t</type>
      <name>conversion_function</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>cooked</name>
    <filename>structcooked.html</filename>
    <member kind="variable">
      <type>raw_t</type>
      <name>rw</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>first_row_printed</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_row_printed</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>first_premapped_pass</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>first_normal_pass</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>first_postmapped_pass</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>first_unused_pass</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>pass_premap</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>stagger_premap</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>pass_postmap</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>stagger_postmap</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>curve_param_t</name>
    <filename>structcurve__param__t.html</filename>
    <member kind="variable">
      <type>stp_parameter_t</type>
      <name>param</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_curve_t **</type>
      <name>defval</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>channel_mask</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>hsl_only</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>color_only</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>debug_msgbuf_t</name>
    <filename>structdebug__msgbuf__t.html</filename>
    <member kind="variable">
      <type>stp_outfunc_t</type>
      <name>ofunc</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>odata</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>data</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>bytes</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>distance_t</name>
    <filename>structdistance__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>dx</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dy</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>r_sq</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>dither</name>
    <filename>structdither.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>src_width</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dst_width</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>spread</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>spread_mask</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>stpi_dither_type</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>adaptive_limit</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>x_aspect</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y_aspect</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>transition</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>offset0_table</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>offset1_table</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>d_cutoff</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_line_was_empty</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ptr_offset</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>error_rows</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>finalized</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_dither_matrix_impl_t</type>
      <name>dither_matrix</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_dither_matrix_impl_t</type>
      <name>transition_matrix</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_dither_channel_t *</type>
      <name>channel</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>channel_count</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>total_channel_count</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned *</type>
      <name>channel_index</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned *</type>
      <name>subchannel_count</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ditherfunc_t *</type>
      <name>ditherfunc</name>
      <anchor>o23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>aux_data</name>
      <anchor>o24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>aux_freefunc</name>
      <anchor>o25</anchor>
      <arglist>)(struct dither *)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>dither_channel</name>
    <filename>structdither__channel.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>randomizer</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>bit_max</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>signif_bits</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>density</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>darkness</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>v</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>o</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>b</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>very_fast</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ink_defn_t *</type>
      <name>ink_list</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>nlevels</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_dither_segment_t *</type>
      <name>ranges</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>error_rows</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int **</type>
      <name>errs</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_dither_matrix_impl_t</type>
      <name>pick</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_dither_matrix_impl_t</type>
      <name>dithermat</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>row_ends</name>
      <anchor>o16</anchor>
      <arglist>[2]</arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>ptr</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>aux_data</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>dither_matrix_impl</name>
    <filename>structdither__matrix__impl.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>base</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>exp</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>x_size</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y_size</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>total_size</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_x</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_x_mod</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_y</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_y_mod</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>index</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>i_own</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>x_offset</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y_offset</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>fast_mask</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned *</type>
      <name>matrix</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>dither_segment</name>
    <filename>structdither__segment.html</filename>
    <member kind="variable">
      <type>stpi_ink_defn_t *</type>
      <name>lower</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ink_defn_t *</type>
      <name>upper</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>range_span</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>value_span</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>is_same_ink</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>is_equal</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>errbuf_t</name>
    <filename>structerrbuf__t.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>data</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>bytes</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>escp2_dropsize_t</name>
    <filename>structescp2__dropsize__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>listname</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>numdropsizes</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const double</type>
      <name>dropsizes</name>
      <anchor>o2</anchor>
      <arglist>[MAX_DROP_SIZES]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>escp2_inkname_t</name>
    <filename>structescp2__inkname__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>inkset_id_t</type>
      <name>inkset</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const channel_set_t *</type>
      <name>channel_set</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>escp2_printer</name>
    <filename>structescp2__printer.html</filename>
    <member kind="variable">
      <type>model_cap_t</type>
      <name>flags</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>nozzles</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>min_nozzles</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>nozzle_separation</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>black_nozzles</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>min_black_nozzles</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>black_nozzle_separation</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>fast_nozzles</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>min_fast_nozzles</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>fast_nozzle_separation</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>physical_channels</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>base_separation</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>resolution_scale</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>max_black_resolution</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>max_hres</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>max_vres</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>min_hres</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>min_vres</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>extra_feed</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>separation_rows</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>pseudo_separation_rows</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>zero_margin_offset</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>initial_vertical_offset</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>black_initial_vertical_offset</name>
      <anchor>o23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>extra_720dpi_separation</name>
      <anchor>o24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_paper_width</name>
      <anchor>o25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_paper_height</name>
      <anchor>o26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>min_paper_width</name>
      <anchor>o27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>min_paper_height</name>
      <anchor>o28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>left_margin</name>
      <anchor>o29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>right_margin</name>
      <anchor>o30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>top_margin</name>
      <anchor>o31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>bottom_margin</name>
      <anchor>o32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_left_margin</name>
      <anchor>o33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_right_margin</name>
      <anchor>o34</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_top_margin</name>
      <anchor>o35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_bottom_margin</name>
      <anchor>o36</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>roll_left_margin</name>
      <anchor>o37</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>roll_right_margin</name>
      <anchor>o38</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>roll_top_margin</name>
      <anchor>o39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>roll_bottom_margin</name>
      <anchor>o40</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_roll_left_margin</name>
      <anchor>o41</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_roll_right_margin</name>
      <anchor>o42</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_roll_top_margin</name>
      <anchor>o43</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>m_roll_bottom_margin</name>
      <anchor>o44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>cd_x_offset</name>
      <anchor>o45</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>cd_y_offset</name>
      <anchor>o46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>cd_page_width</name>
      <anchor>o47</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>cd_page_height</name>
      <anchor>o48</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>alignment_passes</name>
      <anchor>o49</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>alignment_choices</name>
      <anchor>o50</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>alternate_alignment_passes</name>
      <anchor>o51</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>alternate_alignment_choices</name>
      <anchor>o52</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const short *</type>
      <name>dot_sizes</name>
      <anchor>o53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const float *</type>
      <name>densities</name>
      <anchor>o54</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_drop_list_t *</type>
      <name>drops</name>
      <anchor>o55</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const res_t *const *</type>
      <name>reslist</name>
      <anchor>o56</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t *</type>
      <name>inkgroup</name>
      <anchor>o57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const short *</type>
      <name>bits</name>
      <anchor>o58</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const short *</type>
      <name>base_resolutions</name>
      <anchor>o59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const input_slot_list_t *</type>
      <name>input_slots</name>
      <anchor>o60</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const quality_list_t *</type>
      <name>quality_list</name>
      <anchor>o61</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_raw_t *</type>
      <name>preinit_sequence</name>
      <anchor>o62</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_raw_t *</type>
      <name>postinit_remote_sequence</name>
      <anchor>o63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_list_t *const </type>
      <name>printer_weaves</name>
      <anchor>o64</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>escp2_printer_attr_t</name>
    <filename>structescp2__printer__attr__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>attr_name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>bit_shift</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>bit_width</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>escp2_privdata_t</name>
    <filename>structescp2__privdata__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>nozzles</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>min_nozzles</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>nozzle_separation</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>head_offset</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_head_offset</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_management_units</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vertical_units</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>horizontal_units</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>micro_units</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>unit_scale</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>send_zero_pass_advance</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bitwidth</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>drop_size</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ink_resid</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_inkname_t *</type>
      <name>inkname</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>logical_channels</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physical_channels</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>channels_in_use</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char **</type>
      <name>cols</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const physical_subchannel_t **</type>
      <name>channels</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>use_black_parameters</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>use_fast_360</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>advanced_command_set</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>use_extended_commands</name>
      <anchor>o23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const input_slot_t *</type>
      <name>input_slot</name>
      <anchor>o24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_t *</type>
      <name>paper_type</name>
      <anchor>o25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_t *</type>
      <name>paper_adjustment</name>
      <anchor>o26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inkgroup_t *</type>
      <name>ink_group</name>
      <anchor>o27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_raw_t *</type>
      <name>init_sequence</name>
      <anchor>o28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_raw_t *</type>
      <name>deinit_sequence</name>
      <anchor>o29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>model_featureset_t</type>
      <name>command_set</name>
      <anchor>o30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>variable_dots</name>
      <anchor>o31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>has_vacuum</name>
      <anchor>o32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>has_graymode</name>
      <anchor>o33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>base_separation</name>
      <anchor>o34</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>resolution_scale</name>
      <anchor>o35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>printing_resolution</name>
      <anchor>o36</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>separation_rows</name>
      <anchor>o37</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>pseudo_separation_rows</name>
      <anchor>o38</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>extra_720dpi_separation</name>
      <anchor>o39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>horizontal_passes</name>
      <anchor>o40</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physical_xdpi</name>
      <anchor>o41</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const res_t *</type>
      <name>res</name>
      <anchor>o42</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_t *</type>
      <name>printer_weave</name>
      <anchor>o43</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>use_printer_weave</name>
      <anchor>o44</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_left</name>
      <anchor>o45</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_right</name>
      <anchor>o46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_top</name>
      <anchor>o47</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_bottom</name>
      <anchor>o48</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_width</name>
      <anchor>o49</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_height</name>
      <anchor>o50</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_true_height</name>
      <anchor>o51</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>cd_x_offset</name>
      <anchor>o52</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>cd_y_offset</name>
      <anchor>o53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>cd_outer_radius</name>
      <anchor>o54</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>cd_inner_radius</name>
      <anchor>o55</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_height</name>
      <anchor>o56</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_width</name>
      <anchor>o57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_top</name>
      <anchor>o58</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_left</name>
      <anchor>o59</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_scaled_width</name>
      <anchor>o60</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_printed_width</name>
      <anchor>o61</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_scaled_height</name>
      <anchor>o62</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_printed_height</name>
      <anchor>o63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_left_position</name>
      <anchor>o64</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>printed_something</name>
      <anchor>o65</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>initial_vertical_offset</name>
      <anchor>o66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>printing_initial_vertical_offset</name>
      <anchor>o67</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_color</name>
      <anchor>o68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_pass_offset</name>
      <anchor>o69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_pass</name>
      <anchor>o70</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>eventone_t</name>
    <filename>structeventone__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>d2x</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>d2y</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>distance_t</type>
      <name>d_sq</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>aspect</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>unitone_aspect</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physical_aspect</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>diff_factor</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_dither_channel_t *</type>
      <name>dummy_channel</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>float_param_t</name>
    <filename>structfloat__param__t.html</filename>
    <member kind="variable">
      <type>const stp_parameter_t</type>
      <name>param</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>min</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>max</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>defval</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>color_only</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>channel_mask</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ink_channel_t</name>
    <filename>structink__channel__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>listname</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const physical_subchannel_t *</type>
      <name>subchannels</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>n_subchannels</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ink_defn</name>
    <filename>structink__defn.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>range</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>value</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>bits</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ink_list_t</name>
    <filename>structink__list__t.html</filename>
    <member kind="variable">
      <type>const ink_t *</type>
      <name>item</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_items</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>ink_t</name>
    <filename>structink__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>output_type</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>output_channels</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>channel_order</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>output_type</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>rotate_channels</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>inkgroup_t</name>
    <filename>structinkgroup__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>listname</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const inklist_t *const *</type>
      <name>inklists</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>n_inklists</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>inklist_t</name>
    <filename>structinklist__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const escp2_inkname_t *const *</type>
      <name>inknames</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paperlist_t *</type>
      <name>papers</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_list_t *</type>
      <name>paper_adjustments</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const shade_set_t *</type>
      <name>shades</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>n_inks</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>input_slot_list_t</name>
    <filename>structinput__slot__list__t.html</filename>
    <member kind="variable">
      <type>const input_slot_t *</type>
      <name>slots</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_input_slots</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>input_slot_t</name>
    <filename>structinput__slot__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>is_cd</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>is_roll_feed</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>roll_feed_cut_flags</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_raw_t</type>
      <name>init_sequence</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_raw_t</type>
      <name>deinit_sequence</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>laminate_list_t</name>
    <filename>structlaminate__list__t.html</filename>
    <member kind="variable">
      <type>const laminate_t *</type>
      <name>item</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_items</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>laminate_t</name>
    <filename>structlaminate__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_raw_t</type>
      <name>seq</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>lexm_privdata_weave</name>
    <filename>structlexm__privdata__weave.html</filename>
    <member kind="variable">
      <type>const lexmark_inkparam_t *</type>
      <name>ink_parameter</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bidirectional</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>direction</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>hoffset</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>model</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>width</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ydpi</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>xdpi</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physical_xdpi</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_pass_offset</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>jets</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bitwidth</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>horizontal_weave</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>outbuf</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>lexmark_cap_t</name>
    <filename>structlexmark__cap__t.html</filename>
    <member kind="variable">
      <type>Lex_model</type>
      <name>model</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_paper_width</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_paper_height</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>min_paper_width</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>min_paper_height</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_xdpi</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_ydpi</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_quality</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_left</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_right</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_top</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_bottom</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>inks</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>slots</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>features</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>offset_left_border</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>offset_top_border</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>x_raster_res</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y_raster_res</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const lexmark_res_t_array *</type>
      <name>res_parameters</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const lexmark_inkname_t *</type>
      <name>ink_types</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>lum_adjustment</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>hue_adjustment</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>sat_adjustment</name>
      <anchor>o23</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>Lexmark_head_colors</name>
    <filename>structLexmark__head__colors.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>v_start</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>line</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>head_nozzle_start</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>head_nozzle_end</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>used_jets</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>lexmark_inkname_t</name>
    <filename>structlexmark__inkname__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>lexmark_inkparam_t</type>
      <name>ink_parameter</name>
      <anchor>o2</anchor>
      <arglist>[2]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>lexmark_inkparam_t</name>
    <filename>structlexmark__inkparam__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned int</type>
      <name>used_colors</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned int</type>
      <name>pass_length</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>v_top_head_offset</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>h_catridge_offset</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>h_direction_offset</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int *</type>
      <name>head_offset</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="union">
    <name>lexmark_linebufs_t</name>
    <filename>unionlexmark__linebufs__t.html</filename>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>v</name>
      <anchor>o0</anchor>
      <arglist>[NCHANNELS]</arglist>
    </member>
    <member kind="variable">
      <type>lexmark_linebufs_t::@3</type>
      <name>p</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>k</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>c</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>m</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>y</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>C</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>M</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>Y</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="union">
    <name>lexmark_lineoff_t</name>
    <filename>unionlexmark__lineoff__t.html</filename>
    <member kind="variable">
      <type>unsigned long</type>
      <name>v</name>
      <anchor>o0</anchor>
      <arglist>[NCHANNELS]</arglist>
    </member>
    <member kind="variable">
      <type>lexmark_lineoff_t::@2</type>
      <name>p</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>k</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>c</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>m</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>y</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>C</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>M</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long</type>
      <name>Y</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>lexmark_res_t</name>
    <filename>structlexmark__res__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>hres</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vres</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>softweave</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vertical_passes</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vertical_oversample</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>unidirectional</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>resid</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>lut_t</name>
    <filename>structlut__t.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>steps</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>channel_depth</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>image_width</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>in_channels</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>out_channels</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>channels_are_initialized</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>invert_output</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const color_description_t *</type>
      <name>input_color_description</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const color_description_t *</type>
      <name>output_color_description</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const color_correction_t *</type>
      <name>color_correction</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>brightness_correction</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>contrast_correction</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>user_color_correction</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>channel_curves</name>
      <anchor>o13</anchor>
      <arglist>[STP_CHANNEL_LIMIT]</arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>gamma_values</name>
      <anchor>o14</anchor>
      <arglist>[STP_CHANNEL_LIMIT]</arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>print_gamma</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>app_gamma</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>screen_gamma</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>contrast</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>brightness</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>linear_contrast_adjustment</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>printed_colorfunc</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>hue_map</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>lum_map</name>
      <anchor>o23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>sat_map</name>
      <anchor>o24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_cached_curve_t</type>
      <name>gcr_curve</name>
      <anchor>o25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short *</type>
      <name>gray_tmp</name>
      <anchor>o26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short *</type>
      <name>cmy_tmp</name>
      <anchor>o27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short *</type>
      <name>cmyk_tmp</name>
      <anchor>o28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>in_data</name>
      <anchor>o29</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>margins_t</name>
    <filename>structmargins__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>top_margin</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bottom_margin</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>left_margin</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>right_margin</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olymp_pagesize_list_t</name>
    <filename>structolymp__pagesize__list__t.html</filename>
    <member kind="variable">
      <type>const olymp_pagesize_t *</type>
      <name>item</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_items</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olymp_pagesize_t</name>
    <filename>structolymp__pagesize__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>width_pt</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>height_pt</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_pt_left</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_pt_right</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_pt_top</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>border_pt_bottom</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olymp_printsize_list_t</name>
    <filename>structolymp__printsize__list__t.html</filename>
    <member kind="variable">
      <type>const olymp_printsize_t *</type>
      <name>item</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_items</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olymp_printsize_t</name>
    <filename>structolymp__printsize__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>res_name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>pagesize_name</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>width_px</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>height_px</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olymp_resolution_list_t</name>
    <filename>structolymp__resolution__list__t.html</filename>
    <member kind="variable">
      <type>const olymp_resolution_t *</type>
      <name>item</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_items</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olymp_resolution_t</name>
    <filename>structolymp__resolution__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>xdpi</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ydpi</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olympus_cap_t</name>
    <filename>structolympus__cap__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>model</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const ink_list_t *</type>
      <name>inks</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const olymp_resolution_list_t *</type>
      <name>resolution</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const olymp_pagesize_list_t *</type>
      <name>pages</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const olymp_printsize_list_t *</type>
      <name>printsize</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>interlacing</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>block_size</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>features</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>printer_init_func</name>
      <anchor>o8</anchor>
      <arglist>)(stp_vars_t *)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>printer_end_func</name>
      <anchor>o9</anchor>
      <arglist>)(stp_vars_t *)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>plane_init_func</name>
      <anchor>o10</anchor>
      <arglist>)(stp_vars_t *)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>plane_end_func</name>
      <anchor>o11</anchor>
      <arglist>)(stp_vars_t *)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>block_init_func</name>
      <anchor>o12</anchor>
      <arglist>)(stp_vars_t *)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>block_end_func</name>
      <anchor>o13</anchor>
      <arglist>)(stp_vars_t *)</arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>adj_cyan</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>adj_magenta</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>adj_yellow</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const laminate_list_t *</type>
      <name>laminate</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>olympus_privdata_t</name>
    <filename>structolympus__privdata__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>xdpi</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ydpi</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>xsize</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ysize</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char</type>
      <name>plane</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>block_min_x</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>block_min_y</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>block_max_x</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>block_max_y</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>pagesize</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const laminate_t *</type>
      <name>laminate</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>paper_adjustment_list_t</name>
    <filename>structpaper__adjustment__list__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>listname</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>paper_count</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_adjustment_t *</type>
      <name>papers</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>paper_adjustment_t</name>
    <filename>structpaper__adjustment__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>base_density</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>subchannel_cutoff</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>k_transition</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>k_lower</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>k_upper</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>cyan</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>magenta</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>yellow</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>black</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>saturation</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float</type>
      <name>gamma</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>hue_adjustment</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>lum_adjustment</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>sat_adjustment</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>paper_t</name>
    <filename>structpaper__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>media_code</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>base_density</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>k_lower_scale</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>k_upper</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>hue_adjustment</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>lum_adjustment</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>sat_adjustment</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>paper_class_t</type>
      <name>paper_class</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>paper_feed_sequence</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>platen_gap</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>feed_adjustment</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>vacuum_intensity</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>paper_thickness</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>preferred_ink_type</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>preferred_ink_set</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>paper_feed_sequence</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>platen_gap</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>cyan</name>
      <anchor>o23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>magenta</name>
      <anchor>o24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>yellow</name>
      <anchor>o25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>p_cyan</name>
      <anchor>o26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>p_magenta</name>
      <anchor>o27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>p_yellow</name>
      <anchor>o28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>saturation</name>
      <anchor>o29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>gamma</name>
      <anchor>o30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>feed_adjustment</name>
      <anchor>o31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vacuum_intensity</name>
      <anchor>o32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>paper_thickness</name>
      <anchor>o33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>hue_adjustment</name>
      <anchor>o34</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>lum_adjustment</name>
      <anchor>o35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>sat_adjustment</name>
      <anchor>o36</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>paperlist_t</name>
    <filename>structpaperlist__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>listname</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>paper_count</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const paper_t *</type>
      <name>papers</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>pcl_cap_t</name>
    <filename>structpcl__cap__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>model</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>custom_max_width</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>custom_max_height</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>custom_min_width</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>custom_min_height</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>resolutions</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>margins_t</type>
      <name>normal_margins</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>margins_t</type>
      <name>a4_margins</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>color_type</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>stp_printer_type</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const short *</type>
      <name>paper_sizes</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const short *</type>
      <name>paper_types</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const short *</type>
      <name>paper_sources</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>pcl_privdata_t</name>
    <filename>structpcl__privdata__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>do_blank</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>blank_lines</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>comp_buf</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>writefunc</name>
      <anchor>o3</anchor>
      <arglist>)(stp_vars_t *, unsigned char *, int, int)</arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>do_cret</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>do_cretb</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>do_6color</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>height</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>duplex</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>tumble</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>use_crd</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>pcl_t</name>
    <filename>structpcl__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>pcl_name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>pcl_text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>pcl_code</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>p0</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>p1</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>physical_subchannel_t</name>
    <filename>structphysical__subchannel__t.html</filename>
    <member kind="variable">
      <type>short</type>
      <name>color</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>subchannel</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>head_offset</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>channel_density</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>subchannel_scale</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>printer_weave_list_t</name>
    <filename>structprinter__weave__list__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_printer_weaves</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const printer_weave_t *</type>
      <name>printer_weaves</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>printer_weave_t</name>
    <filename>structprinter__weave__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>value</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>quality_list_t</name>
    <filename>structquality__list__t.html</filename>
    <member kind="variable">
      <type>const quality_t *</type>
      <name>qualities</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>n_quals</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>quality_t</name>
    <filename>structquality__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>min_hres</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>min_vres</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>max_hres</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>max_vres</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>desired_hres</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>desired_vres</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>raw</name>
    <filename>structraw.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>separation</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>jets</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>oversampling</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>advancebasis</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>subblocksperpassblock</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>passespersubblock</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_weave_strategy_t</type>
      <name>strategy</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_vars_t *</type>
      <name>v</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>raw_printer</name>
    <filename>structraw__printer.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>output_bits</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>res_t</name>
    <filename>structres__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>hres</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>vres</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>printed_hres</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>printed_vres</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>softweave</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>printer_weave</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short</type>
      <name>vertical_passes</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>shade_segment</name>
    <filename>structshade__segment.html</filename>
    <member kind="variable">
      <type>distance_t</type>
      <name>dis</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>distance_t *</type>
      <name>et_dis</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ink_defn_t</type>
      <name>lower</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_ink_defn_t</type>
      <name>upper</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>share_this_channel</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>shade_t</name>
    <filename>structshade__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>n_shades</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const double</type>
      <name>shades</name>
      <anchor>o1</anchor>
      <arglist>[PHYSICAL_CHANNEL_LIMIT]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_array</name>
    <filename>structstp__array.html</filename>
    <member kind="variable">
      <type>stp_sequence_t *</type>
      <name>data</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>x_size</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y_size</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_cached_curve_t</name>
    <filename>structstp__cached__curve__t.html</filename>
    <member kind="variable">
      <type>stp_curve_t *</type>
      <name>curve</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const double *</type>
      <name>d_cache</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned short *</type>
      <name>s_cache</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>count</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_color</name>
    <filename>structstp__color.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>short_name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>long_name</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_colorfuncs_t *</type>
      <name>colorfuncs</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_colorfuncs_t</name>
    <filename>structstp__colorfuncs__t.html</filename>
    <member kind="variable">
      <type>int(*</type>
      <name>init</name>
      <anchor>o0</anchor>
      <arglist>)(stp_vars_t *v, stp_image_t *image, size_t steps)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>get_row</name>
      <anchor>o1</anchor>
      <arglist>)(stp_vars_t *v, stp_image_t *image, int row, unsigned *zero_mask)</arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_list_t(*</type>
      <name>list_parameters</name>
      <anchor>o2</anchor>
      <arglist>)(const stp_vars_t *v)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>describe_parameter</name>
      <anchor>o3</anchor>
      <arglist>)(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_compdata</name>
    <filename>structstp__compdata.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_copy_data_func_t</type>
      <name>copyfunc</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_free_data_func_t</type>
      <name>freefunc</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>data</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_curve</name>
    <filename>structstp__curve.html</filename>
    <member kind="variable">
      <type>stp_curve_type_t</type>
      <name>curve_type</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_curve_wrap_mode_t</type>
      <name>wrap_mode</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>piecewise</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>recompute_interval</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>gamma</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_sequence_t *</type>
      <name>seq</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double *</type>
      <name>interval</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_curve_point_t</name>
    <filename>structstp__curve__point__t.html</filename>
    <member kind="variable">
      <type>double</type>
      <name>x</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>y</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_dither_matrix_generic</name>
    <filename>structstp__dither__matrix__generic.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>x</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bytes</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>prescaled</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const void *</type>
      <name>data</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_dither_matrix_normal</name>
    <filename>structstp__dither__matrix__normal.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>x</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bytes</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>prescaled</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned *</type>
      <name>data</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_dither_matrix_short</name>
    <filename>structstp__dither__matrix__short.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>x</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bytes</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>prescaled</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned short *</type>
      <name>data</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_dotsize</name>
    <filename>structstp__dotsize.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>bit_pattern</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>value</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_double_bound_t</name>
    <filename>structstp__double__bound__t.html</filename>
    <member kind="variable">
      <type>double</type>
      <name>lower</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>upper</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_family</name>
    <filename>structstp__family.html</filename>
    <member kind="variable">
      <type>const stp_printfuncs_t *</type>
      <name>printfuncs</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_t *</type>
      <name>printer_list</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_image</name>
    <filename>structstp__image.html</filename>
    <member kind="variable">
      <type>void(*</type>
      <name>init</name>
      <anchor>o0</anchor>
      <arglist>)(struct stp_image *image)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>reset</name>
      <anchor>o1</anchor>
      <arglist>)(struct stp_image *image)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>width</name>
      <anchor>o2</anchor>
      <arglist>)(struct stp_image *image)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>height</name>
      <anchor>o3</anchor>
      <arglist>)(struct stp_image *image)</arglist>
    </member>
    <member kind="variable">
      <type>stp_image_status_t(*</type>
      <name>get_row</name>
      <anchor>o4</anchor>
      <arglist>)(struct stp_image *image, unsigned char *data, size_t byte_limit, int row)</arglist>
    </member>
    <member kind="variable">
      <type>const char *(*</type>
      <name>get_appname</name>
      <anchor>o5</anchor>
      <arglist>)(struct stp_image *image)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>conclude</name>
      <anchor>o6</anchor>
      <arglist>)(struct stp_image *image)</arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>rep</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_int_bound_t</name>
    <filename>structstp__int__bound__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>lower</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>upper</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_lineactive_t</name>
    <filename>structstp__lineactive__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>v</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_linebounds_t</name>
    <filename>structstp__linebounds__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>start_pos</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>end_pos</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_linebufs_t</name>
    <filename>structstp__linebufs__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char **</type>
      <name>v</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_linecount_t</name>
    <filename>structstp__linecount__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>v</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_lineoff_t</name>
    <filename>structstp__lineoff__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long *</type>
      <name>v</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_list</name>
    <filename>structstp__list.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>icache</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>length</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_item *</type>
      <name>start</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_item *</type>
      <name>end</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_item *</type>
      <name>cache</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_node_freefunc</type>
      <name>freefunc</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_node_copyfunc</type>
      <name>copyfunc</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_node_namefunc</type>
      <name>namefunc</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_node_namefunc</type>
      <name>long_namefunc</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_node_sortfunc</type>
      <name>sortfunc</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>name_cache</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_item *</type>
      <name>name_cache_node</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>long_name_cache</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_item *</type>
      <name>long_name_cache_node</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_list_item</name>
    <filename>structstp__list__item.html</filename>
    <member kind="variable">
      <type>void *</type>
      <name>data</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_item *</type>
      <name>prev</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_item *</type>
      <name>next</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_module</name>
    <filename>structstp__module.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>version</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>comment</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_module_class_t</type>
      <name>class</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>handle</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>init</name>
      <anchor>o5</anchor>
      <arglist>)(void)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>fini</name>
      <anchor>o6</anchor>
      <arglist>)(void)</arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>syms</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_module_version</name>
    <filename>structstp__module__version.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>major</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>minor</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_mxml_attr_s</name>
    <filename>structstp__mxml__attr__s.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>value</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_mxml_node_s</name>
    <filename>structstp__mxml__node__s.html</filename>
    <member kind="variable">
      <type>stp_mxml_type_t</type>
      <name>type</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_node_t *</type>
      <name>next</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_node_t *</type>
      <name>prev</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_node_t *</type>
      <name>parent</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_node_t *</type>
      <name>child</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_node_t *</type>
      <name>last_child</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_value_t</type>
      <name>value</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_mxml_text_s</name>
    <filename>structstp__mxml__text__s.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>whitespace</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>string</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_mxml_value_s</name>
    <filename>structstp__mxml__value__s.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>num_attrs</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_attr_t *</type>
      <name>attrs</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="union">
    <name>stp_mxml_value_u</name>
    <filename>unionstp__mxml__value__u.html</filename>
    <member kind="variable">
      <type>stp_mxml_element_t</type>
      <name>element</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>integer</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>opaque</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>real</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_mxml_text_t</type>
      <name>text</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_papersize_t</name>
    <filename>structstp__papersize__t.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>comment</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>width</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>height</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>top</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>left</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>bottom</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>right</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_papersize_unit_t</type>
      <name>paper_unit</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_param_string_t</name>
    <filename>structstp__param__string__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_parameter_t</name>
    <filename>structstp__parameter__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>category</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>help</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_type_t</type>
      <name>p_type</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_class_t</type>
      <name>p_class</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_level_t</type>
      <name>p_level</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char</type>
      <name>is_mandatory</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char</type>
      <name>is_active</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char</type>
      <name>channel</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char</type>
      <name>verify_this_parameter</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char</type>
      <name>read_only</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_t::@0</type>
      <name>bounds</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_curve_t *</type>
      <name>curve</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_double_bound_t</type>
      <name>dbl</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_int_bound_t</type>
      <name>integer</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_int_bound_t</type>
      <name>dimension</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_string_list_t *</type>
      <name>str</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_array_t *</type>
      <name>array</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_t::@1</type>
      <name>deflt</name>
      <anchor>o26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_curve_t *</type>
      <name>curve</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>dbl</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>dimension</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>integer</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>boolean</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>str</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_array_t *</type>
      <name>array</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_pass_t</name>
    <filename>structstp__pass__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>pass</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>missingstartrows</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>logicalpassstart</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physpassstart</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physpassend</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>subpass</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_printer</name>
    <filename>structstp__printer.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>driver</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>long_name</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>family</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>manufacturer</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>model</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_printfuncs_t *</type>
      <name>printfuncs</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_vars_t *</type>
      <name>printvars</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_printfuncs_t</name>
    <filename>structstp__printfuncs__t.html</filename>
    <member kind="variable">
      <type>stp_parameter_list_t(*</type>
      <name>list_parameters</name>
      <anchor>o0</anchor>
      <arglist>)(const stp_vars_t *v)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>parameters</name>
      <anchor>o1</anchor>
      <arglist>)(const stp_vars_t *v, const char *name, stp_parameter_t *)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>media_size</name>
      <anchor>o2</anchor>
      <arglist>)(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>imageable_area</name>
      <anchor>o3</anchor>
      <arglist>)(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>limit</name>
      <anchor>o4</anchor>
      <arglist>)(const stp_vars_t *v, int *max_width, int *max_height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>print</name>
      <anchor>o5</anchor>
      <arglist>)(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>describe_resolution</name>
      <anchor>o6</anchor>
      <arglist>)(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="variable">
      <type>const char *(*</type>
      <name>describe_output</name>
      <anchor>o7</anchor>
      <arglist>)(const stp_vars_t *v)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>verify</name>
      <anchor>o8</anchor>
      <arglist>)(stp_vars_t *v)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>start_job</name>
      <anchor>o9</anchor>
      <arglist>)(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="variable">
      <type>int(*</type>
      <name>end_job</name>
      <anchor>o10</anchor>
      <arglist>)(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_raw_t</name>
    <filename>structstp__raw__t.html</filename>
    <member kind="variable">
      <type>size_t</type>
      <name>bytes</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const void *</type>
      <name>data</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_sequence</name>
    <filename>structstp__sequence.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>recompute_range</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>blo</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>bhi</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>rlo</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>rhi</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>size</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double *</type>
      <name>data</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>float *</type>
      <name>float_data</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>long *</type>
      <name>long_data</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned long *</type>
      <name>ulong_data</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>int_data</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned *</type>
      <name>uint_data</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>short *</type>
      <name>short_data</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short *</type>
      <name>ushort_data</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_shade</name>
    <filename>structstp__shade.html</filename>
    <member kind="variable">
      <type>double</type>
      <name>value</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>numsizes</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_dotsize_t *</type>
      <name>dot_sizes</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_vars</name>
    <filename>structstp__vars.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>driver</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>char *</type>
      <name>color_conversion</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>left</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>top</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>width</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>height</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_width</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>page_height</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_list_t *</type>
      <name>params</name>
      <anchor>o8</anchor>
      <arglist>[STP_PARAMETER_TYPE_INVALID]</arglist>
    </member>
    <member kind="variable">
      <type>stp_list_t *</type>
      <name>internal_data</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>outfunc</name>
      <anchor>o10</anchor>
      <arglist>)(void *data, const char *buffer, size_t bytes)</arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>outdata</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void(*</type>
      <name>errfunc</name>
      <anchor>o12</anchor>
      <arglist>)(void *data, const char *buffer, size_t bytes)</arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>errdata</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>verified</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_weave_t</name>
    <filename>structstp__weave__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>row</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>pass</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>jet</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>missingstartrows</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>logicalpassstart</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physpassstart</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>physpassend</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stp_xml_dither_cache_t</name>
    <filename>structstp__xml__dither__cache__t.html</filename>
    <member kind="variable">
      <type>int</type>
      <name>x</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>y</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>filename</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const stp_array_t *</type>
      <name>dither_array</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_channel_group_t</name>
    <filename>structstpi__channel__group__t.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>channel_count</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>total_channels</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>input_channels</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>size_t</type>
      <name>width</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>initialized</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>ink_limit</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned</type>
      <name>max_density</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_channel_t *</type>
      <name>c</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short *</type>
      <name>input_data</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short *</type>
      <name>data</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>black_channel</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_channel_t</name>
    <filename>structstpi__channel__t.html</filename>
    <member kind="variable">
      <type>unsigned</type>
      <name>subchannel_count</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stpi_subchannel_t *</type>
      <name>sc</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short *</type>
      <name>lut</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_dither_algorithm_t</name>
    <filename>structstpi__dither__algorithm__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>id</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_image_type_t</name>
    <filename>structstpi__image__type__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_internal_module_class</name>
    <filename>structstpi__internal__module__class.html</filename>
    <member kind="variable">
      <type>stp_module_class_t</type>
      <name>class</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>description</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_job_mode_t</name>
    <filename>structstpi__job__mode__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_quality_t</name>
    <filename>structstpi__quality__t.html</filename>
    <member kind="variable">
      <type>const char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const char *</type>
      <name>text</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>quality_level</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_softweave</name>
    <filename>structstpi__softweave.html</filename>
    <member kind="variable">
      <type>stp_linebufs_t *</type>
      <name>linebases</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_lineoff_t *</type>
      <name>lineoffsets</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_lineactive_t *</type>
      <name>lineactive</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_linecount_t *</type>
      <name>linecounts</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_linebounds_t *</type>
      <name>linebounds</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_pass_t *</type>
      <name>passes</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_pass_offset</name>
      <anchor>o6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>last_pass</name>
      <anchor>o7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>jets</name>
      <anchor>o8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>virtual_jets</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>separation</name>
      <anchor>o10</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>void *</type>
      <name>weaveparm</name>
      <anchor>o11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>horizontal_weave</name>
      <anchor>o12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vertical_subpasses</name>
      <anchor>o13</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vmod</name>
      <anchor>o14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>oversample</name>
      <anchor>o15</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>repeat_count</name>
      <anchor>o16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ncolors</name>
      <anchor>o17</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>linewidth</name>
      <anchor>o18</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vertical_height</name>
      <anchor>o19</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>firstline</name>
      <anchor>o20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bitwidth</name>
      <anchor>o21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>lineno</name>
      <anchor>o22</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vertical_oversample</name>
      <anchor>o23</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>current_vertical_subpass</name>
      <anchor>o24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>horizontal_width</name>
      <anchor>o25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int *</type>
      <name>head_offset</name>
      <anchor>o26</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>s</name>
      <anchor>o27</anchor>
      <arglist>[STP_MAX_WEAVE]</arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>fold_buf</name>
      <anchor>o28</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned char *</type>
      <name>comp_buf</name>
      <anchor>o29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_weave_t</type>
      <name>wcache</name>
      <anchor>o30</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>rcache</name>
      <anchor>o31</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>vcache</name>
      <anchor>o32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_flushfunc *</type>
      <name>flushfunc</name>
      <anchor>o33</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_fillfunc *</type>
      <name>fillfunc</name>
      <anchor>o34</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_packfunc *</type>
      <name>pack</name>
      <anchor>o35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_compute_linewidth_func *</type>
      <name>compute_linewidth</name>
      <anchor>o36</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_subchannel_t</name>
    <filename>structstpi__subchannel__t.html</filename>
    <member kind="variable">
      <type>double</type>
      <name>value</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>lower</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>upper</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>cutoff</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>unsigned short</type>
      <name>s_density</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>stpi_xml_parse_registry</name>
    <filename>structstpi__xml__parse__registry.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_xml_parse_func</type>
      <name>parse_func</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>value_t</name>
    <filename>structvalue__t.html</filename>
    <member kind="variable">
      <type>char *</type>
      <name>name</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_type_t</type>
      <name>typ</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_parameter_activity_t</type>
      <name>active</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>value_t::@4</type>
      <name>value</name>
      <anchor>o9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>ival</name>
      <anchor>o0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>bval</name>
      <anchor>o1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>double</type>
      <name>dval</name>
      <anchor>o2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_curve_t *</type>
      <name>cval</name>
      <anchor>o3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_array_t *</type>
      <name>aval</name>
      <anchor>o4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>stp_raw_t</type>
      <name>rval</name>
      <anchor>o5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>array</name>
    <title>array</title>
    <filename>group__array.html</filename>
    <member kind="typedef">
      <type>stp_array</type>
      <name>stp_array_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create</name>
      <anchor>ga1</anchor>
      <arglist>(int x_size, int y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_destroy</name>
      <anchor>ga2</anchor>
      <arglist>(stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_copy</name>
      <anchor>ga3</anchor>
      <arglist>(stp_array_t *dest, const stp_array_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_array_t *</type>
      <name>stp_array_create_copy</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_set_size</name>
      <anchor>ga5</anchor>
      <arglist>(stp_array_t *array, int x_size, int y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_get_size</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_array_t *array, int *x_size, int *y_size)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_set_data</name>
      <anchor>ga7</anchor>
      <arglist>(stp_array_t *array, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_array_get_data</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_array_t *array, size_t *size, const double **data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_array_set_point</name>
      <anchor>ga9</anchor>
      <arglist>(stp_array_t *array, int x, int y, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_array_get_point</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_array_t *array, int x, int y, double *data)</arglist>
    </member>
    <member kind="function">
      <type>const stp_sequence_t *</type>
      <name>stp_array_get_sequence</name>
      <anchor>ga11</anchor>
      <arglist>(const stp_array_t *array)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>color</name>
    <title>color</title>
    <filename>group__color.html</filename>
    <class kind="struct">stp_colorfuncs_t</class>
    <class kind="struct">stp_color</class>
    <member kind="typedef">
      <type>stp_color</type>
      <name>stp_color_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_init</name>
      <anchor>ga1</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, size_t steps)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_get_row</name>
      <anchor>ga2</anchor>
      <arglist>(stp_vars_t *v, stp_image_t *image, int row, unsigned *zero_mask)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_color_list_parameters</name>
      <anchor>ga3</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_color_describe_parameter</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_register</name>
      <anchor>ga5</anchor>
      <arglist>(const stp_color_t *color)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_unregister</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_color_t *color)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_color_count</name>
      <anchor>ga7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_name</name>
      <anchor>ga8</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_index</name>
      <anchor>ga9</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stp_color_t *</type>
      <name>stp_get_color_by_colorfuncs</name>
      <anchor>ga10</anchor>
      <arglist>(stp_colorfuncs_t *colorfuncs)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_color_get_name</name>
      <anchor>ga11</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_color_get_long_name</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_color_t *c)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>curve</name>
    <title>curve</title>
    <filename>group__curve.html</filename>
    <class kind="struct">stp_curve_point_t</class>
    <member kind="typedef">
      <type>stp_curve</type>
      <name>stp_curve_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_type_t</name>
      <anchor>ga47</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_TYPE_LINEAR</name>
      <anchor>gga47a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_TYPE_SPLINE</name>
      <anchor>gga47a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_wrap_mode_t</name>
      <anchor>ga48</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_WRAP_NONE</name>
      <anchor>gga48a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_WRAP_AROUND</name>
      <anchor>gga48a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_compose_t</name>
      <anchor>ga49</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_COMPOSE_ADD</name>
      <anchor>gga49a5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_COMPOSE_MULTIPLY</name>
      <anchor>gga49a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_COMPOSE_EXPONENTIATE</name>
      <anchor>gga49a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_curve_bounds_t</name>
      <anchor>ga50</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_BOUNDS_RESCALE</name>
      <anchor>gga50a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_BOUNDS_CLIP</name>
      <anchor>gga50a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_CURVE_BOUNDS_ERROR</name>
      <anchor>gga50a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create</name>
      <anchor>ga1</anchor>
      <arglist>(stp_curve_wrap_mode_t wrap)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_copy</name>
      <anchor>ga2</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_copy</name>
      <anchor>ga3</anchor>
      <arglist>(stp_curve_t *dest, const stp_curve_t *source)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_destroy</name>
      <anchor>ga4</anchor>
      <arglist>(stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_bounds</name>
      <anchor>ga5</anchor>
      <arglist>(stp_curve_t *curve, double low, double high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_get_bounds</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_curve_t *curve, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_wrap_mode_t</type>
      <name>stp_curve_get_wrap</name>
      <anchor>ga7</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_is_piecewise</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_curve_get_range</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_curve_t *curve, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_curve_count_points</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_interpolation_type</name>
      <anchor>ga11</anchor>
      <arglist>(stp_curve_t *curve, stp_curve_type_t itype)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_type_t</type>
      <name>stp_curve_get_interpolation_type</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_data</name>
      <anchor>ga13</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_data_points</name>
      <anchor>ga14</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const stp_curve_point_t *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_float_data</name>
      <anchor>ga15</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const float *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_long_data</name>
      <anchor>ga16</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_ulong_data</name>
      <anchor>ga17</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const unsigned long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_int_data</name>
      <anchor>ga18</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_uint_data</name>
      <anchor>ga19</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const unsigned int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_short_data</name>
      <anchor>ga20</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const short *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_ushort_data</name>
      <anchor>ga21</anchor>
      <arglist>(stp_curve_t *curve, size_t count, const unsigned short *data)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_get_subrange</name>
      <anchor>ga22</anchor>
      <arglist>(const stp_curve_t *curve, size_t start, size_t count)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_subrange</name>
      <anchor>ga23</anchor>
      <arglist>(stp_curve_t *curve, const stp_curve_t *range, size_t start)</arglist>
    </member>
    <member kind="function">
      <type>const double *</type>
      <name>stp_curve_get_data</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const stp_curve_point_t *</type>
      <name>stp_curve_get_data_points</name>
      <anchor>ga25</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const float *</type>
      <name>stp_curve_get_float_data</name>
      <anchor>ga26</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const long *</type>
      <name>stp_curve_get_long_data</name>
      <anchor>ga27</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned long *</type>
      <name>stp_curve_get_ulong_data</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const int *</type>
      <name>stp_curve_get_int_data</name>
      <anchor>ga29</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned int *</type>
      <name>stp_curve_get_uint_data</name>
      <anchor>ga30</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const short *</type>
      <name>stp_curve_get_short_data</name>
      <anchor>ga31</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned short *</type>
      <name>stp_curve_get_ushort_data</name>
      <anchor>ga32</anchor>
      <arglist>(const stp_curve_t *curve, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const stp_sequence_t *</type>
      <name>stp_curve_get_sequence</name>
      <anchor>ga33</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_gamma</name>
      <anchor>ga34</anchor>
      <arglist>(stp_curve_t *curve, double f_gamma)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_curve_get_gamma</name>
      <anchor>ga35</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_set_point</name>
      <anchor>ga36</anchor>
      <arglist>(stp_curve_t *curve, size_t where, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_get_point</name>
      <anchor>ga37</anchor>
      <arglist>(const stp_curve_t *curve, size_t where, double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_interpolate_value</name>
      <anchor>ga38</anchor>
      <arglist>(const stp_curve_t *curve, double where, double *result)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_resample</name>
      <anchor>ga39</anchor>
      <arglist>(stp_curve_t *curve, size_t points)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_rescale</name>
      <anchor>ga40</anchor>
      <arglist>(stp_curve_t *curve, double scale, stp_curve_compose_t mode, stp_curve_bounds_t bounds_mode)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_write</name>
      <anchor>ga41</anchor>
      <arglist>(FILE *file, const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_curve_write_string</name>
      <anchor>ga42</anchor>
      <arglist>(const stp_curve_t *curve)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_stream</name>
      <anchor>ga43</anchor>
      <arglist>(FILE *fp)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_file</name>
      <anchor>ga44</anchor>
      <arglist>(const char *file)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_curve_create_from_string</name>
      <anchor>ga45</anchor>
      <arglist>(const char *string)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_curve_compose</name>
      <anchor>ga46</anchor>
      <arglist>(stp_curve_t **retval, stp_curve_t *a, stp_curve_t *b, stp_curve_compose_t mode, int points)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>intl_internal</name>
    <title>intl-internal</title>
    <filename>group__intl__internal.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>textdomain</name>
      <anchor>ga0</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>gettext</name>
      <anchor>ga1</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dgettext</name>
      <anchor>ga2</anchor>
      <arglist>(Domain, Message)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dcgettext</name>
      <anchor>ga3</anchor>
      <arglist>(Domain, Message, Type)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>bindtextdomain</name>
      <anchor>ga4</anchor>
      <arglist>(Domain, Directory)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>_</name>
      <anchor>ga5</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>N_</name>
      <anchor>ga6</anchor>
      <arglist>(String)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>intl</name>
    <title>intl</title>
    <filename>group__intl.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>textdomain</name>
      <anchor>ga0</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>gettext</name>
      <anchor>ga1</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dgettext</name>
      <anchor>ga2</anchor>
      <arglist>(Domain, Message)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>dcgettext</name>
      <anchor>ga3</anchor>
      <arglist>(Domain, Message, Type)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>bindtextdomain</name>
      <anchor>ga4</anchor>
      <arglist>(Domain, Directory)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>_</name>
      <anchor>ga5</anchor>
      <arglist>(String)</arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>N_</name>
      <anchor>ga6</anchor>
      <arglist>(String)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>version</name>
    <title>version</title>
    <filename>group__version.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_MAJOR_VERSION</name>
      <anchor>ga7</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_MINOR_VERSION</name>
      <anchor>ga8</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_MICRO_VERSION</name>
      <anchor>ga9</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_CURRENT_INTERFACE</name>
      <anchor>ga10</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_BINARY_AGE</name>
      <anchor>ga11</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_INTERFACE_AGE</name>
      <anchor>ga12</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>GIMPPRINT_CHECK_VERSION</name>
      <anchor>ga13</anchor>
      <arglist>(major, minor, micro)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_check_version</name>
      <anchor>ga6</anchor>
      <arglist>(unsigned int required_major, unsigned int required_minor, unsigned int required_micro)</arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_major_version</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_minor_version</name>
      <anchor>ga1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_micro_version</name>
      <anchor>ga2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_current_interface</name>
      <anchor>ga3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_binary_age</name>
      <anchor>ga4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const unsigned int</type>
      <name>gimpprint_interface_age</name>
      <anchor>ga5</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>image</name>
    <title>image</title>
    <filename>group__image.html</filename>
    <class kind="struct">stp_image</class>
    <member kind="define">
      <type>#define</type>
      <name>STP_CHANNEL_LIMIT</name>
      <anchor>ga8</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_image</type>
      <name>stp_image_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_image_status_t</name>
      <anchor>ga9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_IMAGE_STATUS_OK</name>
      <anchor>gga9a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_IMAGE_STATUS_ABORT</name>
      <anchor>gga9a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_init</name>
      <anchor>ga1</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_reset</name>
      <anchor>ga2</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_image_width</name>
      <anchor>ga3</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_image_height</name>
      <anchor>ga4</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>stp_image_status_t</type>
      <name>stp_image_get_row</name>
      <anchor>ga5</anchor>
      <arglist>(stp_image_t *image, unsigned char *data, size_t limit, int row)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_image_get_appname</name>
      <anchor>ga6</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_image_conclude</name>
      <anchor>ga7</anchor>
      <arglist>(stp_image_t *image)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>papersize</name>
    <title>papersize</title>
    <filename>group__papersize.html</filename>
    <class kind="struct">stp_papersize_t</class>
    <member kind="enumeration">
      <name>stp_papersize_unit_t</name>
      <anchor>ga5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_ENGLISH_STANDARD</name>
      <anchor>gga5a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_METRIC_STANDARD</name>
      <anchor>gga5a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_ENGLISH_EXTENDED</name>
      <anchor>gga5a2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PAPERSIZE_METRIC_EXTENDED</name>
      <anchor>gga5a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_known_papersizes</name>
      <anchor>ga0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_name</name>
      <anchor>ga1</anchor>
      <arglist>(const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_size</name>
      <anchor>ga2</anchor>
      <arglist>(int length, int width)</arglist>
    </member>
    <member kind="function">
      <type>const stp_papersize_t *</type>
      <name>stp_get_papersize_by_index</name>
      <anchor>ga3</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_default_media_size</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>printer</name>
    <title>printer</title>
    <filename>group__printer.html</filename>
    <class kind="struct">stp_printfuncs_t</class>
    <class kind="struct">stp_family</class>
    <member kind="typedef">
      <type>stp_printer</type>
      <name>stp_printer_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>stp_family</type>
      <name>stp_family_t</name>
      <anchor>ga1</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_printer_model_count</name>
      <anchor>ga2</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_index</name>
      <anchor>ga3</anchor>
      <arglist>(int idx)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_long_name</name>
      <anchor>ga4</anchor>
      <arglist>(const char *long_name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer_by_driver</name>
      <anchor>ga5</anchor>
      <arglist>(const char *driver)</arglist>
    </member>
    <member kind="function">
      <type>const stp_printer_t *</type>
      <name>stp_get_printer</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_printer_index_by_driver</name>
      <anchor>ga7</anchor>
      <arglist>(const char *driver)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_long_name</name>
      <anchor>ga8</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_driver</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_family</name>
      <anchor>ga10</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_printer_get_manufacturer</name>
      <anchor>ga11</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_printer_get_model</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>const stp_vars_t *</type>
      <name>stp_printer_get_defaults</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_printer_defaults</name>
      <anchor>ga14</anchor>
      <arglist>(stp_vars_t *v, const stp_printer_t *p)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_print</name>
      <anchor>ga15</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_start_job</name>
      <anchor>ga16</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_end_job</name>
      <anchor>ga17</anchor>
      <arglist>(const stp_vars_t *v, stp_image_t *image)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_model_id</name>
      <anchor>ga18</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_verify_printer_params</name>
      <anchor>ga19</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_family_register</name>
      <anchor>ga20</anchor>
      <arglist>(stp_list_t *family)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_family_unregister</name>
      <anchor>ga21</anchor>
      <arglist>(stp_list_t *family)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_initialize_printer_defaults</name>
      <anchor>ga22</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_printer_list_parameters</name>
      <anchor>ga23</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_printer_describe_parameter</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_describe_output</name>
      <anchor>ga25</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>sequence</name>
    <title>sequence</title>
    <filename>group__sequence.html</filename>
    <member kind="typedef">
      <type>stp_sequence</type>
      <name>stp_sequence_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create</name>
      <anchor>ga1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_destroy</name>
      <anchor>ga2</anchor>
      <arglist>(stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_copy</name>
      <anchor>ga3</anchor>
      <arglist>(stp_sequence_t *dest, const stp_sequence_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_sequence_t *</type>
      <name>stp_sequence_create_copy</name>
      <anchor>ga4</anchor>
      <arglist>(const stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_bounds</name>
      <anchor>ga5</anchor>
      <arglist>(stp_sequence_t *sequence, double low, double high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_bounds</name>
      <anchor>ga6</anchor>
      <arglist>(const stp_sequence_t *sequence, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_range</name>
      <anchor>ga7</anchor>
      <arglist>(const stp_sequence_t *sequence, double *low, double *high)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_size</name>
      <anchor>ga8</anchor>
      <arglist>(stp_sequence_t *sequence, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_sequence_get_size</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_sequence_t *sequence)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_data</name>
      <anchor>ga10</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_subrange</name>
      <anchor>ga11</anchor>
      <arglist>(stp_sequence_t *sequence, size_t where, size_t size, const double *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_sequence_get_data</name>
      <anchor>ga12</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *size, const double **data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_point</name>
      <anchor>ga13</anchor>
      <arglist>(stp_sequence_t *sequence, size_t where, double data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_get_point</name>
      <anchor>ga14</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t where, double *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_float_data</name>
      <anchor>ga15</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const float *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_long_data</name>
      <anchor>ga16</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_ulong_data</name>
      <anchor>ga17</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const unsigned long *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_int_data</name>
      <anchor>ga18</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_uint_data</name>
      <anchor>ga19</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const unsigned int *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_short_data</name>
      <anchor>ga20</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const short *data)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_sequence_set_ushort_data</name>
      <anchor>ga21</anchor>
      <arglist>(stp_sequence_t *sequence, size_t count, const unsigned short *data)</arglist>
    </member>
    <member kind="function">
      <type>const float *</type>
      <name>stp_sequence_get_float_data</name>
      <anchor>ga22</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const long *</type>
      <name>stp_sequence_get_long_data</name>
      <anchor>ga23</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned long *</type>
      <name>stp_sequence_get_ulong_data</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const int *</type>
      <name>stp_sequence_get_int_data</name>
      <anchor>ga25</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned int *</type>
      <name>stp_sequence_get_uint_data</name>
      <anchor>ga26</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const short *</type>
      <name>stp_sequence_get_short_data</name>
      <anchor>ga27</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
    <member kind="function">
      <type>const unsigned short *</type>
      <name>stp_sequence_get_ushort_data</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_sequence_t *sequence, size_t *count)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>util</name>
    <title>util</title>
    <filename>group__util.html</filename>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_LUT</name>
      <anchor>ga31</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_COLORFUNC</name>
      <anchor>ga32</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_INK</name>
      <anchor>ga33</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PS</name>
      <anchor>ga34</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PCL</name>
      <anchor>ga35</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_ESCP2</name>
      <anchor>ga36</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_CANON</name>
      <anchor>ga37</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_LEXMARK</name>
      <anchor>ga38</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_WEAVE_PARAMS</name>
      <anchor>ga39</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_ROWS</name>
      <anchor>ga40</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_MARK_FILE</name>
      <anchor>ga41</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_LIST</name>
      <anchor>ga42</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_MODULE</name>
      <anchor>ga43</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PATH</name>
      <anchor>ga44</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PAPER</name>
      <anchor>ga45</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_PRINTERS</name>
      <anchor>ga46</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_XML</name>
      <anchor>ga47</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_VARS</name>
      <anchor>ga48</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_OLYMPUS</name>
      <anchor>ga49</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_CURVE</name>
      <anchor>ga50</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_DBG_CURVE_ERRORS</name>
      <anchor>ga51</anchor>
      <arglist></arglist>
    </member>
    <member kind="define">
      <type>#define</type>
      <name>STP_SAFE_FREE</name>
      <anchor>ga52</anchor>
      <arglist>(x)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_init</name>
      <anchor>ga0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_set_output_codeset</name>
      <anchor>ga1</anchor>
      <arglist>(const char *codeset)</arglist>
    </member>
    <member kind="function">
      <type>stp_curve_t *</type>
      <name>stp_read_and_compose_curves</name>
      <anchor>ga2</anchor>
      <arglist>(const char *s1, const char *s2, stp_curve_compose_t comp, size_t piecewise_point_count)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_abort</name>
      <anchor>ga3</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_prune_inactive_options</name>
      <anchor>ga4</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_zprintf</name>
      <anchor>ga5</anchor>
      <arglist>(const stp_vars_t *v, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_zfwrite</name>
      <anchor>ga6</anchor>
      <arglist>(const char *buf, size_t bytes, size_t nitems, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_putc</name>
      <anchor>ga7</anchor>
      <arglist>(int ch, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put16_le</name>
      <anchor>ga8</anchor>
      <arglist>(unsigned short sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put16_be</name>
      <anchor>ga9</anchor>
      <arglist>(unsigned short sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put32_le</name>
      <anchor>ga10</anchor>
      <arglist>(unsigned int sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_put32_be</name>
      <anchor>ga11</anchor>
      <arglist>(unsigned int sh, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_puts</name>
      <anchor>ga12</anchor>
      <arglist>(const char *s, const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_send_command</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_vars_t *v, const char *command, const char *format,...)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_erputc</name>
      <anchor>ga14</anchor>
      <arglist>(int ch)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_eprintf</name>
      <anchor>ga15</anchor>
      <arglist>(const stp_vars_t *v, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_erprintf</name>
      <anchor>ga16</anchor>
      <arglist>(const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_asprintf</name>
      <anchor>ga17</anchor>
      <arglist>(char **strp, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_catprintf</name>
      <anchor>ga18</anchor>
      <arglist>(char **strp, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>unsigned long</type>
      <name>stp_get_debug_level</name>
      <anchor>ga19</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_dprintf</name>
      <anchor>ga20</anchor>
      <arglist>(unsigned long level, const stp_vars_t *v, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_deprintf</name>
      <anchor>ga21</anchor>
      <arglist>(unsigned long level, const char *format,...) __attribute__((format(__printf__</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_init_debug_messages</name>
      <anchor>ga22</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_flush_debug_messages</name>
      <anchor>ga23</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_malloc</name>
      <anchor>ga24</anchor>
      <arglist>(size_t)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_zalloc</name>
      <anchor>ga25</anchor>
      <arglist>(size_t)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_realloc</name>
      <anchor>ga26</anchor>
      <arglist>(void *ptr, size_t)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_free</name>
      <anchor>ga27</anchor>
      <arglist>(void *ptr)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_strlen</name>
      <anchor>ga28</anchor>
      <arglist>(const char *s)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_strndup</name>
      <anchor>ga29</anchor>
      <arglist>(const char *s, int n)</arglist>
    </member>
    <member kind="function">
      <type>char *</type>
      <name>stp_strdup</name>
      <anchor>ga30</anchor>
      <arglist>(const char *s)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>vars</name>
    <title>vars</title>
    <filename>group__vars.html</filename>
    <class kind="struct">stp_raw_t</class>
    <class kind="struct">stp_double_bound_t</class>
    <class kind="struct">stp_int_bound_t</class>
    <class kind="struct">stp_parameter_t</class>
    <member kind="typedef">
      <type>stp_vars</type>
      <name>stp_vars_t</name>
      <anchor>ga0</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void *</type>
      <name>stp_parameter_list_t</name>
      <anchor>ga1</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>const void *</type>
      <name>stp_const_parameter_list_t</name>
      <anchor>ga2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>stp_outfunc_t</name>
      <anchor>ga3</anchor>
      <arglist>)(void *data, const char *buffer, size_t bytes)</arglist>
    </member>
    <member kind="typedef">
      <type>void *(*</type>
      <name>stp_copy_data_func_t</name>
      <anchor>ga4</anchor>
      <arglist>)(void *)</arglist>
    </member>
    <member kind="typedef">
      <type>void(*</type>
      <name>stp_free_data_func_t</name>
      <anchor>ga5</anchor>
      <arglist>)(void *)</arglist>
    </member>
    <member kind="typedef">
      <type>stp_compdata</type>
      <name>compdata_t</name>
      <anchor>ga6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_type_t</name>
      <anchor>ga132</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_STRING_LIST</name>
      <anchor>gga132a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_INT</name>
      <anchor>gga132a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_BOOLEAN</name>
      <anchor>gga132a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_DOUBLE</name>
      <anchor>gga132a10</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_CURVE</name>
      <anchor>gga132a11</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_FILE</name>
      <anchor>gga132a12</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_RAW</name>
      <anchor>gga132a13</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_ARRAY</name>
      <anchor>gga132a14</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_DIMENSION</name>
      <anchor>gga132a15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_TYPE_INVALID</name>
      <anchor>gga132a16</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_class_t</name>
      <anchor>ga133</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_FEATURE</name>
      <anchor>gga133a17</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_OUTPUT</name>
      <anchor>gga133a18</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_CORE</name>
      <anchor>gga133a19</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_CLASS_INVALID</name>
      <anchor>gga133a20</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_level_t</name>
      <anchor>ga134</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_BASIC</name>
      <anchor>gga134a21</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED</name>
      <anchor>gga134a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED1</name>
      <anchor>gga134a23</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED2</name>
      <anchor>gga134a24</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED3</name>
      <anchor>gga134a25</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_ADVANCED4</name>
      <anchor>gga134a26</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_INTERNAL</name>
      <anchor>gga134a27</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_EXTERNAL</name>
      <anchor>gga134a28</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_LEVEL_INVALID</name>
      <anchor>gga134a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_activity_t</name>
      <anchor>ga135</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_INACTIVE</name>
      <anchor>gga135a30</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_DEFAULTED</name>
      <anchor>gga135a31</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>STP_PARAMETER_ACTIVE</name>
      <anchor>gga135a32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <name>stp_parameter_verify_t</name>
      <anchor>ga136</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PARAMETER_BAD</name>
      <anchor>gga136a33</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PARAMETER_OK</name>
      <anchor>gga136a34</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PARAMETER_INACTIVE</name>
      <anchor>gga136a35</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>stp_vars_t *</type>
      <name>stp_vars_create</name>
      <anchor>ga7</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_vars_copy</name>
      <anchor>ga8</anchor>
      <arglist>(stp_vars_t *dest, const stp_vars_t *source)</arglist>
    </member>
    <member kind="function">
      <type>stp_vars_t *</type>
      <name>stp_vars_create_copy</name>
      <anchor>ga9</anchor>
      <arglist>(const stp_vars_t *source)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_vars_destroy</name>
      <anchor>ga10</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_driver</name>
      <anchor>ga11</anchor>
      <arglist>(stp_vars_t *v, const char *val)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_driver_n</name>
      <anchor>ga12</anchor>
      <arglist>(stp_vars_t *v, const char *val, int bytes)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_driver</name>
      <anchor>ga13</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_color_conversion</name>
      <anchor>ga14</anchor>
      <arglist>(stp_vars_t *v, const char *val)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_color_conversion_n</name>
      <anchor>ga15</anchor>
      <arglist>(stp_vars_t *v, const char *val, int bytes)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_color_conversion</name>
      <anchor>ga16</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_left</name>
      <anchor>ga17</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_left</name>
      <anchor>ga18</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_top</name>
      <anchor>ga19</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_top</name>
      <anchor>ga20</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_width</name>
      <anchor>ga21</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_width</name>
      <anchor>ga22</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_height</name>
      <anchor>ga23</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_height</name>
      <anchor>ga24</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_page_width</name>
      <anchor>ga25</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_page_width</name>
      <anchor>ga26</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_page_height</name>
      <anchor>ga27</anchor>
      <arglist>(stp_vars_t *v, int val)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_page_height</name>
      <anchor>ga28</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_outfunc</name>
      <anchor>ga29</anchor>
      <arglist>(stp_vars_t *v, stp_outfunc_t val)</arglist>
    </member>
    <member kind="function">
      <type>stp_outfunc_t</type>
      <name>stp_get_outfunc</name>
      <anchor>ga30</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_errfunc</name>
      <anchor>ga31</anchor>
      <arglist>(stp_vars_t *v, stp_outfunc_t val)</arglist>
    </member>
    <member kind="function">
      <type>stp_outfunc_t</type>
      <name>stp_get_errfunc</name>
      <anchor>ga32</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_outdata</name>
      <anchor>ga33</anchor>
      <arglist>(stp_vars_t *v, void *val)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_get_outdata</name>
      <anchor>ga34</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_errdata</name>
      <anchor>ga35</anchor>
      <arglist>(stp_vars_t *v, void *val)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_get_errdata</name>
      <anchor>ga36</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_merge_printvars</name>
      <anchor>ga37</anchor>
      <arglist>(stp_vars_t *user, const stp_vars_t *print)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_get_parameter_list</name>
      <anchor>ga38</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>stp_parameter_list_count</name>
      <anchor>ga39</anchor>
      <arglist>(stp_const_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_find</name>
      <anchor>ga40</anchor>
      <arglist>(stp_const_parameter_list_t list, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_list_param</name>
      <anchor>ga41</anchor>
      <arglist>(stp_const_parameter_list_t list, size_t item)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_destroy</name>
      <anchor>ga42</anchor>
      <arglist>(stp_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_parameter_list_create</name>
      <anchor>ga43</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_add_param</name>
      <anchor>ga44</anchor>
      <arglist>(stp_parameter_list_t list, const stp_parameter_t *item)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_list_t</type>
      <name>stp_parameter_list_copy</name>
      <anchor>ga45</anchor>
      <arglist>(stp_const_parameter_list_t list)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_list_append</name>
      <anchor>ga46</anchor>
      <arglist>(stp_parameter_list_t list, stp_const_parameter_list_t append)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_describe_parameter</name>
      <anchor>ga47</anchor>
      <arglist>(const stp_vars_t *v, const char *name, stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_parameter_description_destroy</name>
      <anchor>ga48</anchor>
      <arglist>(stp_parameter_t *description)</arglist>
    </member>
    <member kind="function">
      <type>const stp_parameter_t *</type>
      <name>stp_parameter_find_in_settings</name>
      <anchor>ga49</anchor>
      <arglist>(const stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter</name>
      <anchor>ga50</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter_n</name>
      <anchor>ga51</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter</name>
      <anchor>ga52</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter_n</name>
      <anchor>ga53</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_float_parameter</name>
      <anchor>ga54</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_int_parameter</name>
      <anchor>ga55</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_dimension_parameter</name>
      <anchor>ga56</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_boolean_parameter</name>
      <anchor>ga57</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_curve_parameter</name>
      <anchor>ga58</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_curve_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_array_parameter</name>
      <anchor>ga59</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_array_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_raw_parameter</name>
      <anchor>ga60</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const void *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_scale_float_parameter</name>
      <anchor>ga61</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double scale)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_string_parameter</name>
      <anchor>ga62</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_string_parameter_n</name>
      <anchor>ga63</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_file_parameter</name>
      <anchor>ga64</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_file_parameter_n</name>
      <anchor>ga65</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const char *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_float_parameter</name>
      <anchor>ga66</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_int_parameter</name>
      <anchor>ga67</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_dimension_parameter</name>
      <anchor>ga68</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_boolean_parameter</name>
      <anchor>ga69</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_curve_parameter</name>
      <anchor>ga70</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_curve_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_array_parameter</name>
      <anchor>ga71</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const stp_array_t *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_default_raw_parameter</name>
      <anchor>ga72</anchor>
      <arglist>(stp_vars_t *v, const char *parameter, const void *value, size_t bytes)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_string_parameter</name>
      <anchor>ga73</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const char *</type>
      <name>stp_get_file_parameter</name>
      <anchor>ga74</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>stp_get_float_parameter</name>
      <anchor>ga75</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_int_parameter</name>
      <anchor>ga76</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_dimension_parameter</name>
      <anchor>ga77</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_boolean_parameter</name>
      <anchor>ga78</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_curve_t *</type>
      <name>stp_get_curve_parameter</name>
      <anchor>ga79</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_array_t *</type>
      <name>stp_get_array_parameter</name>
      <anchor>ga80</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>const stp_raw_t *</type>
      <name>stp_get_raw_parameter</name>
      <anchor>ga81</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_string_parameter</name>
      <anchor>ga82</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_file_parameter</name>
      <anchor>ga83</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_float_parameter</name>
      <anchor>ga84</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_int_parameter</name>
      <anchor>ga85</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_dimension_parameter</name>
      <anchor>ga86</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_boolean_parameter</name>
      <anchor>ga87</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_curve_parameter</name>
      <anchor>ga88</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_array_parameter</name>
      <anchor>ga89</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_clear_raw_parameter</name>
      <anchor>ga90</anchor>
      <arglist>(stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_string_parameter_active</name>
      <anchor>ga91</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_file_parameter_active</name>
      <anchor>ga92</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_float_parameter_active</name>
      <anchor>ga93</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_int_parameter_active</name>
      <anchor>ga94</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_dimension_parameter_active</name>
      <anchor>ga95</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_boolean_parameter_active</name>
      <anchor>ga96</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_curve_parameter_active</name>
      <anchor>ga97</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_array_parameter_active</name>
      <anchor>ga98</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_raw_parameter_active</name>
      <anchor>ga99</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_string_parameter</name>
      <anchor>ga100</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_file_parameter</name>
      <anchor>ga101</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_float_parameter</name>
      <anchor>ga102</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_int_parameter</name>
      <anchor>ga103</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_dimension_parameter</name>
      <anchor>ga104</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_boolean_parameter</name>
      <anchor>ga105</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_curve_parameter</name>
      <anchor>ga106</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_array_parameter</name>
      <anchor>ga107</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_check_raw_parameter</name>
      <anchor>ga108</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, stp_parameter_activity_t active)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_string_parameter_active</name>
      <anchor>ga109</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_file_parameter_active</name>
      <anchor>ga110</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_float_parameter_active</name>
      <anchor>ga111</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_int_parameter_active</name>
      <anchor>ga112</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_dimension_parameter_active</name>
      <anchor>ga113</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_boolean_parameter_active</name>
      <anchor>ga114</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_curve_parameter_active</name>
      <anchor>ga115</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_array_parameter_active</name>
      <anchor>ga116</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_activity_t</type>
      <name>stp_get_raw_parameter_active</name>
      <anchor>ga117</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_media_size</name>
      <anchor>ga118</anchor>
      <arglist>(const stp_vars_t *v, int *width, int *height)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_imageable_area</name>
      <anchor>ga119</anchor>
      <arglist>(const stp_vars_t *v, int *left, int *right, int *bottom, int *top)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_get_size_limit</name>
      <anchor>ga120</anchor>
      <arglist>(const stp_vars_t *v, int *max_width, int *max_height, int *min_width, int *min_height)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_describe_resolution</name>
      <anchor>ga121</anchor>
      <arglist>(const stp_vars_t *v, int *x, int *y)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_verify</name>
      <anchor>ga122</anchor>
      <arglist>(stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>const stp_vars_t *</type>
      <name>stp_default_settings</name>
      <anchor>ga123</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_allocate_component_data</name>
      <anchor>ga124</anchor>
      <arglist>(stp_vars_t *v, const char *name, stp_copy_data_func_t copyfunc, stp_free_data_func_t freefunc, void *data)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_destroy_component_data</name>
      <anchor>ga125</anchor>
      <arglist>(stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>stp_get_component_data</name>
      <anchor>ga126</anchor>
      <arglist>(const stp_vars_t *v, const char *name)</arglist>
    </member>
    <member kind="function">
      <type>stp_parameter_verify_t</type>
      <name>stp_verify_parameter</name>
      <anchor>ga127</anchor>
      <arglist>(const stp_vars_t *v, const char *parameter, int quiet)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>stp_get_verified</name>
      <anchor>ga128</anchor>
      <arglist>(const stp_vars_t *v)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_set_verified</name>
      <anchor>ga129</anchor>
      <arglist>(stp_vars_t *v, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_copy_options</name>
      <anchor>ga130</anchor>
      <arglist>(stp_vars_t *vd, const stp_vars_t *vs)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stp_fill_parameter_settings</name>
      <anchor>ga131</anchor>
      <arglist>(stp_parameter_t *desc, const stp_parameter_t *param)</arglist>
    </member>
  </compound>
  <compound kind="group">
    <name>util_internal</name>
    <title>util-internal</title>
    <filename>group__util__internal.html</filename>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_paper</name>
      <anchor>ga0</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_dither</name>
      <anchor>ga1</anchor>
      <arglist>(void)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>stpi_init_printer</name>
      <anchor>ga2</anchor>
      <arglist>(void)</arglist>
    </member>
  </compound>
</tagfile>
