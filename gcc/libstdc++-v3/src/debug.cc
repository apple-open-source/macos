/* APPLE LOCAL file libstdc++ debug mode */
// Debugging mode support code -*- C++ -*-

// Copyright (C) 2003
// Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
// USA.

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

#include <debug/debug.h>
#include <debug/safe_sequence.h>
#include <debug/safe_iterator.h>
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cctype>

using namespace std;

namespace std
{
  namespace __debug
  {
    const char* _S_debug_messages[] = 
    {
/* __dbg_msg_valid_range */         "function requires a valid iterator range [%1.name;, %2.name;)",
/* __dbg_msg_insert_singular */     "attempt to insert into container with a singular iterator",
/* __dbg_msg_insert_different */    "attempt to insert into container with an iterator from a different container",
/* __dbg_msg_erase_bad */           "attempt to erase from container with a %2.state; iterator",
/* __dbg_msg_erase_different */     "attempt to erase from container with an iterator from a different container",
/* __dbg_msg_subscript_oob */       "attempt to subscript container with out-of-bounds index %2;, but container only holds %3; elements",
/* __dbg_msg_empty */               "attempt to access an element in an empty container",
/* __dbg_msg_unsorted */            "elements in iterator range [%1.name;, %2.name;) are not sorted",
/* __dbg_msg_unsorted_pred */       "elements in iterator range [%1.name;, %2.name;) are not sorted according to the predicate %3;",
/* __dbg_msg_not_heap */            "elements in iterator range [%1.name;, %2.name;) do not form a heap",
/* __dbg_msg_not_heap_pred */       "elements in iterator range [%1.name;, %2.name;) do not form a heap with respect to the predicate %3;",
/* __dbg_msg_bad_bitset_write */    "attempt to write through a singular bitset reference",
/* __dbg_msg_bad_bitset_read */     "attempt to read from a singular bitset reference",
/* __dbg_msg_bad_bitset_flip */     "attempt to flip a singular bitset reference",
/* __dbg_msg_self_splice */         "attempt to splice a list into itself",
/* __dbg_msg_splice_alloc */        "attempt to splice lists with inequal allocators",
/* __dbg_msg_splice_bad */          "attempt to splice elements referenced by a %1.state; iterator",
/* __dbg_msg_splice_other */        "attempt to splice an iterator from a different container",
/* __dbg_msg_splice_overlap */      "splice destination %1.name; occurs within source range [%2.name;, %3.name;)",
/* __dbg_msg_init_singular */       "attempt to initialize an iterator that will immediately become singular",
/* __dbg_msg_init_copy_singular */  "attempt to copy-construct an iterator from a singular iterator",
/* __dbg_msg_init_const_singular */ "attempt to construct a constant iterator from a singular mutable iterator",
/* __dbg_msg_copy_singular */       "attempt to copy from a singular iterator",
/* __dbg_msg_bad_deref */           "attempt to dereference a %1.state; iterator",
/* __dbg_msg_bad_inc */             "attempt to increment a %1.state; iterator",
/* __dbg_msg_bad_dec */             "attempt to decrement a %1.state; iterator",
/* __dbg_msg_iter_subscript_oob */  "attempt to subscript a %1.state; iterator %2; step from its current position, which falls outside its dereferenceable range",
/* __dbg_msg_advance_oob */         "attempt to advance a %1.state; iterator %2; steps, which falls outside its valid range",
/* __dbg_msg_retreat_oob */         "attempt to retreat a %1.state; iterator %2; steps, which falls outside its valid range",
/* __dbg_msg_iter_compare_bad */    "attempt to compare a %1.state; iterator to a %2.state; iterator",
/* __dbg_msg_compare_different */   "attempt to compare iterators from different sequences",
/* __dbg_msg_iter_order_bad */      "attempt to order a %1.state; iterator to a %2.state; iterator",
/* __dbg_msg_order_different */     "attempt to order iterators from different sequences",
/* __dbg_msg_distance_bad */        "attempt to compute the difference between a %1.state; iterator to a %2.state; iterator",
/* __dbg_msg_distance_different */  "attempt to compute the different between two iterators from different sequences",
/* __dbg_msg_deref_istream */       "attempt to dereference an end-of-stream istream_iterator",
/* __dbg_msg_inc_istream */         "attempt to increment an end-of-stream istream_iterator",
/* __dbg_msg_output_ostream */      "attempt to output via an ostream_iterator with no associated stream",
/* __dbg_msg_deref_istreambuf */    "attempt to dereference an end-of-stream istreambuf_iterator (this is a GNU extension)",
/* __dbg_msg_inc_istreambuf */      "attempt to increment an end-of-stream istreambuf_iterator"
    };

    void 
    _Safe_sequence_base::
    _M_detach_all()
    {
      for (_Safe_iterator_base* iterator = _M_iterators; iterator; )
      {
	_Safe_iterator_base* old = iterator;
	iterator = iterator->_M_next;
	old->_M_attach(0, false);
      }

      for (_Safe_iterator_base* iterator = _M_const_iterators; iterator; )
      {
	_Safe_iterator_base* old = iterator;
	iterator = iterator->_M_next;
	old->_M_attach(0, true);
      }
    }

    void 
    _Safe_sequence_base::
    _M_detach_singular()
    {
      for (_Safe_iterator_base* iterator = _M_iterators; iterator; )
      {
	_Safe_iterator_base* old = iterator;
	iterator = iterator->_M_next;
	if (old->_M_singular())
	  old->_M_attach(0, false);
      }

      for (_Safe_iterator_base* iterator = _M_const_iterators; iterator; )
      {
	_Safe_iterator_base* old = iterator;
	iterator = iterator->_M_next;
	if (old->_M_singular())
	  old->_M_attach(0, true);
      }
    }

    void 
    _Safe_sequence_base::
    _M_revalidate_singular()
    {
      for (_Safe_iterator_base* iterator = _M_iterators; iterator;
	   iterator = iterator->_M_next)
      {
	iterator->_M_version = _M_version;
	iterator = iterator->_M_next;
      }

      for (_Safe_iterator_base* iterator = _M_const_iterators; iterator;
	   iterator = iterator->_M_next)
      {
	iterator->_M_version = _M_version;
	iterator = iterator->_M_next;
      }
    }

    void 
    _Safe_sequence_base::
    _M_swap(_Safe_sequence_base& __x)
    {
      swap(_M_iterators, __x._M_iterators);
      swap(_M_const_iterators, __x._M_const_iterators);
      swap(_M_version, __x._M_version);
      for (_Safe_iterator_base* iter = _M_iterators; iter; iter = iter->_M_next)
	iter->_M_sequence = this;
      for (_Safe_iterator_base* iter = __x._M_iterators; iter; iter = iter->_M_next)
	iter->_M_sequence = &__x;
      for (_Safe_iterator_base* iter = _M_const_iterators; iter; iter = iter->_M_next)
	iter->_M_sequence = this;
      for (_Safe_iterator_base* iter = __x._M_const_iterators; iter; iter = iter->_M_next)
	iter->_M_sequence = &__x;
    }

    void 
    _Safe_iterator_base::
    _M_attach(_Safe_sequence_base* __seq, bool __constant)
    {
      _M_detach();
  
      // Attach to the new sequence (if there is one)
      if (__seq)
      {
	_M_sequence = __seq;
	_M_version = _M_sequence->_M_version;
	_M_prior = 0;
	if (__constant)
	{
	  _M_next = _M_sequence->_M_const_iterators;
	  if (_M_next)
	    _M_next->_M_prior = this;
	  _M_sequence->_M_const_iterators = this;
	}
	else
	{
	  _M_next = _M_sequence->_M_iterators;
	  if (_M_next)
	    _M_next->_M_prior = this;
	  _M_sequence->_M_iterators = this;
	}
      }
    }

    void 
    _Safe_iterator_base::
    _M_detach()
    {
      if (_M_sequence)
      {
	// Remove us from this sequence's list
	if (_M_prior) _M_prior->_M_next = _M_next;
	if (_M_next)  _M_next->_M_prior = _M_prior;
	
	if (_M_sequence->_M_const_iterators == this)
	  _M_sequence->_M_const_iterators = _M_next;
	if (_M_sequence->_M_iterators == this)
	  _M_sequence->_M_iterators = _M_next;
      }
      
      _M_sequence = 0;
      _M_version = 0;
      _M_prior = 0;
      _M_next = 0;
    }
    
    bool
    _Safe_iterator_base::
    _M_singular() const
    { return !_M_sequence || _M_version != _M_sequence->_M_version; }
    
    bool
    _Safe_iterator_base::
    _M_can_compare(const _Safe_iterator_base& __x) const
    {
      return (! _M_singular() && !__x._M_singular()
	      && _M_sequence == __x._M_sequence);
    }

    void
    _Error_formatter::_Parameter::
    _M_print_field(const _Error_formatter* __formatter,
		   const char* __name) const
    {
      assert(this->_M_kind != _Parameter::__unused_param);
      const int bufsize = 64;
      char buf[bufsize];

      if (_M_kind == __iterator)
	{
	  if (strcmp(__name, "name") == 0)
	    {
	      assert(_M_variant._M_iterator._M_name);
	      __formatter->_M_print_word(_M_variant._M_iterator._M_name);
	    }
	  else if (strcmp(__name, "address") == 0)
	    {
	      snprintf(buf, bufsize, "%p", _M_variant._M_iterator._M_address);
	      __formatter->_M_print_word(buf);
	    }
	  else if (strcmp(__name, "type") == 0)
	    {
	      assert(_M_variant._M_iterator._M_type);
	      // TBD: demangle!
	      __formatter->_M_print_word(_M_variant._M_iterator._M_type->name());
	    }
	  else if (strcmp(__name, "constness") == 0)
	    {
	      static const char* __constness_names[__last_constness] =
	      {
		"<unknown>",
		"constant",
		"mutable"
	      };
	      __formatter->_M_print_word(__constness_names[_M_variant._M_iterator._M_constness]);
	    }
	  else if (strcmp(__name, "state") == 0)
	    {
	      static const char* __state_names[__last_state] = 
	      {
		"<unknown>",
		"singular",
		"dereferenceable (start-of-sequence)",
		"dereferenceable",
		"past-the-end"
	      };
	      __formatter->_M_print_word(__state_names[_M_variant._M_iterator._M_state]);
	    }
	  else if (strcmp(__name, "sequence") == 0)
	    {
	      assert(_M_variant._M_iterator._M_sequence);
	      snprintf(buf, bufsize, "%p", _M_variant._M_iterator._M_sequence);
	      __formatter->_M_print_word(buf);
	    }
	  else if (strcmp(__name, "seq_type") == 0)
	    {
	      // TBD: demangle!
	      assert(_M_variant._M_iterator._M_seq_type);
	      __formatter->_M_print_word(_M_variant._M_iterator._M_seq_type->name());
	    }
	  else
	    assert(false);
	}
      else if (_M_kind == __sequence)
	{
	  if (strcmp(__name, "name") == 0)
	    {
	      assert(_M_variant._M_sequence._M_name);
	      __formatter->_M_print_word(_M_variant._M_sequence._M_name);
	    }
	  else if (strcmp(__name, "address") == 0)
	    {
	      assert(_M_variant._M_sequence._M_address);
	      snprintf(buf, bufsize, "%p", _M_variant._M_sequence._M_address);
	      __formatter->_M_print_word(buf);
	    }
	  else if (strcmp(__name, "type") == 0)
	    {
	      // TBD: demangle!
	      assert(_M_variant._M_sequence._M_type);
	      __formatter->_M_print_word(_M_variant._M_sequence._M_type->name());
	    }
	  else
	    assert(false);
	}
      else if (_M_kind == __integer)
	{
	  if (strcmp(__name, "name") == 0)
	    {
	      assert(_M_variant._M_integer._M_name);
	      __formatter->_M_print_word(_M_variant._M_integer._M_name);
	    }
	  else
	    assert(false);
	}
      else if (_M_kind == __string)
	{
	  if (strcmp(__name, "name") == 0)
	    {
	      assert(_M_variant._M_string._M_name);
	      __formatter->_M_print_word(_M_variant._M_string._M_name);
	    }
	  else
	    assert(false);
	}
      else
	{
	  assert(false);
	}
    }

    void
    _Error_formatter::_Parameter::
    _M_print_description(const _Error_formatter* __formatter) const
    {
      const int bufsize = 128;
      char buf[bufsize];

      if (_M_kind == __iterator)
	{
	  __formatter->_M_print_word("iterator ");
	  if (_M_variant._M_iterator._M_name)
	    {
	      snprintf(buf, bufsize, "\"%s\" ", 
		       _M_variant._M_iterator._M_name);
	      __formatter->_M_print_word(buf);
	    }

	  snprintf(buf, bufsize, "@ 0x%p {\n", 
		   _M_variant._M_iterator._M_address);
	  __formatter->_M_print_word(buf);
	  if (_M_variant._M_iterator._M_type)
	    {
	      __formatter->_M_print_word("type = ");
	      _M_print_field(__formatter, "type");

	      if (_M_variant._M_iterator._M_constness != __unknown_constness)
		{
		  __formatter->_M_print_word(" (");
		  _M_print_field(__formatter, "constness");
		  __formatter->_M_print_word(" iterator)");
		}
	      __formatter->_M_print_word(";\n");
	    }
	  
	  if (_M_variant._M_iterator._M_state != __unknown_state)
	    {
	      __formatter->_M_print_word("  state = ");
	      _M_print_field(__formatter, "state");
	      __formatter->_M_print_word(";\n");
	    }

	  if (_M_variant._M_iterator._M_sequence)
	    {
	      __formatter->_M_print_word("  references sequence ");
	      if (_M_variant._M_iterator._M_seq_type)
		{
		  __formatter->_M_print_word("with type `");
		  _M_print_field(__formatter, "seq_type");
		  __formatter->_M_print_word("' ");
		}
	      
	      snprintf(buf, bufsize, "@ 0x%p\n", 
		       _M_variant._M_sequence._M_address);
	      __formatter->_M_print_word(buf);
	    }
	  __formatter->_M_print_word("}\n");
	}
      else if (_M_kind == __sequence)
	{
	  __formatter->_M_print_word("sequence ");
	  if (_M_variant._M_sequence._M_name)
	    {
	      snprintf(buf, bufsize, "\"%s\" ", 
		       _M_variant._M_sequence._M_name);
	      __formatter->_M_print_word(buf);
	    }

	  snprintf(buf, bufsize, "@ 0x%p {\n", 
		   _M_variant._M_sequence._M_address);
	  __formatter->_M_print_word(buf);

	  if (_M_variant._M_sequence._M_type)
	    {
	      __formatter->_M_print_word("  type = ");
	      _M_print_field(__formatter, "type");
	      __formatter->_M_print_word(";\n");
	    }	  
	  __formatter->_M_print_word("}\n");
	}
    }

    const _Error_formatter&
    _Error_formatter::_M_message(_Debug_msg_id __id) const
    {
      return this->_M_message(_S_debug_messages[__id]);
    }

    void
    _Error_formatter::_M_error() const
    {
      const int bufsize = 128;
      char buf[bufsize];

      // Emit file & line number information
      _M_column = 1;
      _M_wordwrap = false;
      if (_M_file)
	{
	  snprintf(buf, bufsize, "%s:", _M_file);
	  _M_print_word(buf);
	  _M_column += strlen(buf);
	}

      if (_M_line > 0)
	{
	  snprintf(buf, bufsize, "%u:", _M_line);
	  _M_print_word(buf);
	  _M_column += strlen(buf);
	}

      _M_wordwrap = true;
      _M_print_word("error: ");

      // Print the error message
      assert(_M_text);
      _M_print_string(_M_text);
      _M_print_word(".\n");

      // Emit descriptions of the objects involved in the operation
      _M_wordwrap = false;
      bool has_noninteger_parameters = false;
      for (unsigned int i = 0; i < _M_num_parameters; ++i)
	{
	  if (_M_parameters[i]._M_kind == _Parameter::__iterator
	      || _M_parameters[i]._M_kind == _Parameter::__sequence)
	    {
	      if (!has_noninteger_parameters)
		{
		  _M_first_line = true;
		  _M_print_word("\nObjects involved in the operation:\n");
		  has_noninteger_parameters = true;
		}
	      _M_parameters[i]._M_print_description(this);
	    }
	}

      abort();
    }

    void 
    _Error_formatter::_M_print_word(const char* __word) const
    {
      if (!_M_wordwrap) 
	{
	  fprintf(stderr, "%s", __word);
	  return;
	}

      size_t __length = strlen(__word);
      if (__length == 0)
	return;

      if ((_M_column + __length < _M_max_length)
	  || (__length >= _M_max_length && _M_column == 1)) 
	{
	  // If this isn't the first line, indent
	  if (_M_column == 1 && !_M_first_line)
	    {
	      char spacing[_M_indent + 1];
	      for (int i = 0; i < _M_indent; ++i)
		spacing[i] = ' ';
	      spacing[_M_indent] = '\0';
	      fprintf(stderr, "%s", spacing);
	      _M_column += _M_indent;
	    }

	  fprintf(stderr, "%s", __word);
	  _M_column += __length;

	  if (__word[__length - 1] == '\n') {
	    _M_first_line = false;
	    _M_column = 1;
	  }
	}
      else
	{
	  _M_column = 1;
	  _M_print_word("\n");
	  _M_print_word(__word);
	}
    }

  void
  _Error_formatter::
  _M_print_string(const char* __string) const
  {
    const char* __start = __string;
    const char* __end = __start;
    const int bufsize = 128;
    char buf[bufsize];

    while (*__start)
      {
	if (*__start != '%')
	  {
	    // [__start, __end) denotes the next word
	    __end = __start;
	    while (isalnum(*__end)) ++__end;
	    if (__start == __end) ++__end;
	    if (isspace(*__end)) ++__end;

	    assert(__end - __start + 1< bufsize);
	    snprintf(buf, __end - __start + 1, "%s", __start);
	    _M_print_word(buf);
	    __start = __end;

	    // Skip extra whitespace
	    while (*__start == ' ') ++__start;

	    continue;
	  } 
	
	++__start;
	assert(*__start);
	if (*__start == '%')
	  {
	    _M_print_word("%");
	    ++__start;
	    continue;
	  }
	
	// Get the parameter number
	assert(*__start >= '1' && *__start <= '9');
	size_t param = *__start - '0';
	--param;
	assert(param < _M_num_parameters);
	
	// '.' separates the parameter number from the field
	// name, if there is one.
	++__start;
	if (*__start != '.')
	  {
	    assert(*__start == ';');
	    ++__start;
	    buf[0] = '\0';
	    if (_M_parameters[param]._M_kind == _Parameter::__integer)
	      {
		snprintf(buf, bufsize, "%d", 
			 _M_parameters[param]._M_variant._M_integer._M_value);
		_M_print_word(buf);
	      }
	    else if (_M_parameters[param]._M_kind == _Parameter::__string)
	      _M_print_string(_M_parameters[param]._M_variant._M_string._M_value);
	    continue;
	  }

	// Extract the field name we want
	enum { max_field_len = 16 };
	char field[max_field_len];
	int field_idx = 0;
	++__start;
	while (*__start != ';')
	  {
	    assert(*__start);
	    assert(field_idx < max_field_len-1);
	    field[field_idx++] = *__start++;
	  }
	++__start;
	field[field_idx] = 0;
	
	_M_parameters[param]._M_print_field(this, field);		  
      }
  }

  } // namespace __debug
} // namespace std
