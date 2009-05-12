/**
 * Name: synopsys
 * Description: Synopsys dc shell scripting language
 *
 * Author: Brian Silveira  (brian@nortel.ca)
 *	   Hartley Horwitz (hars@nortel.ca)
 */

state synopsys extends HighlightEntry
{
  /*
   * Synopsys allows globing...so pick out constructs like
   * /foo/bar/* and just print them out.  i.e. don't treat the
   * ending like a comment!
   */
  /[A-z0-9_-]\/\*/{
    language_print($0);
  }


  /* Comments.
   * Synopsys DC-shell uses C-style comments
   */
  /\/\*/ {
    comment_face (true);
    language_print ($0);
    call (c_comment);
    comment_face (false);
  }

  /* String constants. */
  /\"/ {
    string_face (true);
    language_print ($0);
    call (c_string);
    string_face (false);
  }

  /* I use make-regexp in emacs which uses a list of strings to
     generate a	 regular expression.
     (setq synopsys-keywords-enscript
      '("alias" "all_connected" "analyze" "balance_buffer"
	"balance_registers" "break" "cd" "change_link" "change_names"
	"characterize" "check_design" "check_test" "check_timing"
	"continue" "compile" "copy_design" "create_clock" "drive_of"
	"echo" "elaborate" "else" "exit" "get_attribute" "get_license"
	"get_unix_variable" "group" "group_path" "include"
	"insert_scan" "insert_test" "link" "list_designs"
	"list_instances" "list_libs" "load_of" "quit" "read"
	"read_lib" "read_timing" "remove_attribute"
	"remove_constraint" "remove_design" "remove_input_delay"
	"remove_lib" "remove_clock" "remove_cell" "remove_license"
	"remove_output_delay" "remove_unconnected_ports"
	"rename_design" "reoptimize_design" "report_area"
	"report_attribute" "report_cell" "report_constraint"
	"report_design" "report_design_lib" "report_hierarchy"
	"report_internal_loads" "report_lib" "report_multicycles"
	"report_net" "report_port" "report_power" "report_reference"
	"report_resources" "report_test" "report_timing"
	"reset_design" "set_attribute" "set_boundary_optimization"
	"set_clock_skew" "set_dont_touch" "set_dont_touch_network"
	"set_dont_use" "set_drive" "set_driving_cell" "set_equal"
	"set_disable_timing" "set_false_path" "set_flatten"
	"set_implementation" "set_fix_hold" "set_input_delay"
	"set_load" "set_logic_one" "set_logic_zero" "set_max_area"
	"set_max_capacitance" "set_max_fanout" "set_max_transition"
	"set_multicycle_path" "set_operating_conditions"
	"set_output_delay" "set_scan_style" "set_signal_type"
	"set_structure" "set_test_methodology" "set_unconnected"
	"set_wire_load" "unalias" "sh" "ungroup" "uniquify"
	"update_lib" "which" "write" "write_constraints"
	"write_script" "write_timing" "if" "foreach" "find" "while"
	"-all" "-all_violators" "-allowed" "-attributes" "-base_name"
	"-boundary_optimization" "-cell" "-cell_name" "-clock"
	"-context" "-connections" "-constraints" "-delay" "-design"
	"-design_name" "-depth" "-drive" "-except" "-f" "-fall_delay"
	"-flat" "-format" "-from" "-hierarchy" "-hier" "-hold"
	"-incremental_mapping" "-ideal" "-ignored" "-in_place"
	"-logic" "-library" "-map_effort" "-mode" "-max_paths"
	"-max_scan_chain_length" "-no_disable" "-methodology" "-name"
	"-net" "-new_name" "-none" "-nosplit" "-nworst" "-output"
	"-path" "-parameters" "-period" "-pin_load" "-propagated"
	"-reference" "-rise_delay" "-rules" "-skew" "-setup"
	"-through" "-to" "-type" "-uncertainty" "-plus_uncertainty"
	"-minus_uncertainty" "-update" "-verify" "-verbose"
	"-waveform" "-wire_load" "-work" "-weight" "-worst"
	"actel_qbar_opto" "actel_seq_opto" "auto_link_disable"
	"auto_link_options" "auto_wire_load_selection"
	"bc_enable_chaining" "bc_enable_multi_cycle"
	"bc_enable_speculative_execution" "bc_fsm_coding_style"
	"bc_time_all_sequential_op_bindings" "bus_extraction_style"
	"bus_inference_style" "bus_naming_style"
	"change_names_dont_change_bus_members"
	"change_names_update_inst_tree" "command_log_file" "company"
	"compatibility_version"
	"compile_assume_fully_decoded_three_state_busses"
	"compile_create_mux_op_hierarchy"
	"compile_default_critical_range"
	"compile_disable_area_opt_during_inplace_opt"
	"compile_disable_hierarchical_inverter_opt"
	"compile_dont_touch_annotated_cell_during_inplace_opt"
	"compile_fix_multiple_port_nets"
	"compile_ignore_area_during_inplace_opt"
	"compile_ignore_footprint_during_inplace_opt"
	"compile_implementation_selection"
	"compile_inplace_changed_list_file_name"
	"compile_instance_name_prefix" "compile_instance_name_suffix"
	"compile_mux_no_boundary_optimization"
	"compile_negative_logic_methodology"
	"compile_no_new_cells_at_top_level"
	"compile_ok_to_buffer_during_inplace_opt"
	"compile_preserve_subdesign_interfaces"
	"compile_preserve_sync_resets"
	"compile_update_annotated_delays_during_inplace_opt"
	"compile_use_fast_delay_mode" "compile_use_low_timing_effort"
	"context_check_status" "current_design" "current_instance"
	"dc_shell_status" "default_name_rules" "design_library_file"
	"designer" "duplicate_ports" "echo_include_commands"
	"enable_page_mode" "exit_delete_filename_log_file"
	"filename_log_file" "find_converts_name_lists"
	"find_ignore_case" "hdl_keep_licenses" "hdl_naming_threshold"
	"hdl_preferred_license" "hdl_variables"
	"hdlin_advisor_directory" "hdlin_auto_save_templates"
	"hdlin_check_no_latch"
	"hdlin_dont_infer_mux_for_resource_sharing"
	"hdlin_enable_advisor" "hdlin_ff_always_async_set_reset"
	"hdlin_ff_always_sync_set_reset" "hdlin_files"
	"hdlin_hide_resource_line_numbers" "hdlin_infer_mux"
	"hdlin_keep_feedback" "hdlin_keep_inv_feedback"
	"hdlin_mux_size_limit" "hdlin_reg_report_length"
	"hdlin_replace_synthetic" "hdlin_report_inferred_modules"
	"hdlin_resource_allocation" "hdlin_resource_implementation"
	"hdlin_source_to_gates_mode" "hdlin_sync_set_reset"
	"hdlin_synch_set_reset" "hdlin_translate_off_skip_text"
	"link_force_case" "link_library"
	"port_complement_naming_style"
	"reoptimize_design_changed_list_file_name"
	"sdfin_fall_cell_delay_type" "sdfin_fall_net_delay_type"
	"sdfin_min_fall_cell_delay" "sdfin_min_fall_net_delay"
	"sdfin_min_rise_cell_delay" "sdfin_min_rise_net_delay"
	"sdfin_rise_cell_delay_type" "sdfin_rise_net_delay_type"
	"sdfin_top_instance_name"
	"sdfout_allow_non_positive_constraints"
	"sdfout_min_fall_cell_delay" "sdfout_min_fall_net_delay"
	"sdfout_min_rise_cell_delay" "sdfout_min_rise_net_delay"
	"sdfout_time_scale" "sdfout_top_instance_name"
	"sdfout_write_to_output" "search_path" "shell_prompt"
	"suppress_errors" "synlib_dont_get_license"
	"syntax_check_status" "synthetic_library" "target_library"
	"uniquify_naming_style" "verbose_messages"))
	*/

  /\ba(l(ias|l_connected)|nalyze|uto_(link_(disable|options)\
|wire_load_selection))|b(alance_(buffer|registers)\
|c_(enable_(chaining|multi_cycle|speculative_execution)\
|fsm_coding_style|time_all_sequential_op_bindings)|reak\
|us_(extraction_style|inference_style|naming_style))\
|c(d|h(a(nge_(link|names(|_(dont_change_bus_members|update_inst_tree)))\
|racterize)|eck_(design|t(est|iming)))|o(m(mand_log_file|p(a(ny\
|tibility_version)|ile(|_(assume_fully_decoded_three_state_busses\
|create_mux_op_hierarchy|d(efault_critical_range\
|isable_(area_opt_during_inplace_opt|hierarchical_inverter_opt)\
|ont_touch_annotated_cell_during_inplace_opt)|fix_multiple_port_nets\
|i(gnore_(area_during_inplace_opt|footprint_during_inplace_opt)\
|mplementation_selection|n(place_changed_list_file_name\
|stance_name_(prefix|suffix)))|mux_no_boundary_optimization\
|n(egative_logic_methodology|o_new_cells_at_top_level)\
|ok_to_buffer_during_inplace_opt|preserve_s(ubdesign_interfaces\
|ync_resets)|u(pdate_annotated_delays_during_inplace_opt\
|se_(fast_delay_mode|low_timing_effort))))))|nt(ext_check_status|inue)\
|py_design)|reate_clock|urrent_(design|instance))|d(c_shell_status\
|e(fault_name_rules|sign(_library_file|er))|rive_of|uplicate_ports)|e(cho(\
|_include_commands)|l(aborate|se)|nable_page_mode|xit(\
|_delete_filename_log_file))|f(i(lename_log_file|nd(|_(converts_name_lists\
|ignore_case)))|oreach)|g(et_(attribute|license|unix_variable)|roup(\
|_path))|hdl(_(keep_licenses|naming_threshold|preferred_license|variables)\
|in_(a(dvisor_directory|uto_save_templates)|check_no_latch\
|dont_infer_mux_for_resource_sharing|enable_advisor\
|f(f_always_(async_set_reset|sync_set_reset)|iles)\
|hide_resource_line_numbers|infer_mux|keep_(feedback|inv_feedback)\
|mux_size_limit|re(g_report_length|p(lace_synthetic|ort_inferred_modules)\
|source_(allocation|implementation))|s(ource_to_gates_mode|ync(_set_reset\
|h_set_reset))|translate_off_skip_text))|i(f|n(clude|sert_(scan|test)))\
|l(i(nk(|_(force_case|library))|st_(designs|instances|libs))|oad_of)\
|port_complement_naming_style|quit|re(ad(|_(lib|timing))|move_(attribute\
|c(ell|lock|onstraint)|design|input_delay|li(b|cense)|output_delay\
|unconnected_ports)|name_design|optimize_design(|_changed_list_file_name)\
|port_(a(rea|ttribute)|c(ell|onstraint)|design(|_lib)|hierarchy\
|internal_loads|lib|multicycles|net|po(rt|wer)|re(ference|sources)|t(est\
|iming))|set_design)|s(df(in_(fall_(cell_delay_type|net_delay_type)\
|min_(fall_(cell_delay|net_delay)|rise_(cell_delay|net_delay))\
|rise_(cell_delay_type|net_delay_type)|top_instance_name)\
|out_(allow_non_positive_constraints|min_(fall_(cell_delay|net_delay)\
|rise_(cell_delay|net_delay))|t(ime_scale|op_instance_name)\
|write_to_output))|e(arch_path|t_(attribute|boundary_optimization\
|clock_skew|d(isable_timing|ont_(touch(|_network)|use)|riv(e|ing_cell))\
|equal|f(alse_path|ix_hold|latten)|i(mplementation|nput_delay)|lo(ad\
|gic_(one|zero))|m(ax_(area|capacitance|fanout|transition)|ulticycle_path)\
|o(perating_conditions|utput_delay)|s(can_style|ignal_type|tructure)\
|test_methodology|unconnected|wire_load))|h(|ell_prompt)|uppress_errors\
|yn(lib_dont_get_license|t(ax_check_status|hetic_library)))|target_library\
|u(n(alias|group|iquify(|_naming_style))|pdate_lib)|verbose_messages\
|w(hi(ch|le)|rite(|_(constraints|script|timing)))\b/{
    keyword_face (true);
    language_print ($0);
    keyword_face (false);
  }
}


/*
Local variables:
mode: c
End:
*/
