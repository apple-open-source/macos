# -*- mode:ruby; indent-tabs-mode:nil; coding:utf-8 -*-
require 'test/unit'
require 'osx/cocoa'

def with_kcode(k)
  cur = $KCODE
  $KCODE = k
  yield
ensure
  $KCODE = cur
end

class TC_ObjcString < Test::Unit::TestCase

  def setup
    @nsstr = OSX::NSString.stringWithString('NSString')
  end

  # to_str convenience

  def test_autoconv
    path = OSX::NSString.stringWithString(__FILE__)
    assert_nothing_raised('NSString treat as RubyString with "to_str"') {
      open(path) {"no operation"}
    }
  end

  # comparison between Ruby String and Cocoa String

  def test_comparison
    # receiver: OSX::NSString
    assert_equal(0, @nsstr <=> 'NSString', '1-1.NSStr <=> Str -> true')
    assert_not_equal(0, @nsstr <=> 'RBString', '1-2.NSStr <=> Str -> false')
    assert(@nsstr == 'NSString', '1-3.NSStr == Str -> true')
    assert(!(@nsstr == 'RBString'), '1-4.NSStr == Str -> false')
    # receiver: String
    assert_equal(0, 'NSString' <=> @nsstr, '2-1.Str <=> NSStr -> true')
    assert_not_equal(0, 'nsstring' <=> @nsstr, '2-2.Str <=> NSStr -> false')
    assert('NSString' == @nsstr, '2-3.Str == NSStr -> true')
    assert(!('RBString' == @nsstr), '2-4.Str == NSStr -> false')
  end

  def test_length
    with_kcode('utf-8') do
      assert_equal  7,  OSX::NSString.stringWithString('日本語の文字列').length
      assert_equal 11, OSX::NSString.stringWithString('English+日本語').length # Japanese
      assert_equal 15, OSX::NSString.stringWithString('English+العربية').length # Arabic
      assert_equal 11, OSX::NSString.stringWithString('English+한국어').length # Hungle
      assert_equal 18, OSX::NSString.stringWithString('English+Российская').length # Russian
    end
  end

  # forwarding to Ruby String

  def test_respond_to
    assert_respond_to(@nsstr, :ocm_send, 'should respond to "OSX::ObjcID#ocm_send"')
    assert_respond_to(@nsstr, :+, 'should respond to "String#+"')
    assert(!@nsstr.respond_to?(:_xxx_not_defined_method_xxx_), 
      'should not respond to undefined method in String')
  end

  def test_call_string_method
    str = ""
    assert_nothing_raised() {str = @nsstr + 'Appended'}
    assert_equal('NSStringAppended', str) 
  end

  def test_immutable
    assert_raise(OSX::OCException, 'cannot modify immutable string') {
      @nsstr.gsub!(/S/, 'X')
    }
    assert_equal('NSString', @nsstr.to_s, 'value not changed on error(gsub!)')
    assert_raise(OSX::OCException, 'cannot modify immutable string') {
      @nsstr << 'Append'
    }
    assert_equal('NSString', @nsstr.to_s, 'value not changed on error(<<!)')
  end

  def test_mutable
    str = OSX::NSMutableString.stringWithString('NSMutableString')
    assert_nothing_raised('can modify mutable string') {
      str.gsub!(/S/, 'X')
    }
    assert_equal('NXMutableXtring', str.to_s)
  end
  
  # NSString duck typing
  
  def alloc_nsstring(s)
    OSX::NSMutableString.stringWithString(s)
  end
  
  def test_format
    s = 'abc %s abc'
    n = alloc_nsstring(s)
    args = 'hoge'
    assert_equal(s % args, n % args)
    s = 'abc %d,%o abc'
    n = alloc_nsstring(s)
    args = [10,10]
    assert_equal(s % args, n % args)
  end

  def test_times
    s = 'foo'
    n = alloc_nsstring(s)
    s = s * 5
    n = n * 5
    assert_equal(s, n)

    s = 'foo'
    n = alloc_nsstring(s)
    s = s * 0
    n = n * 0
    assert_equal(s, n)
  end
  
  def test_times_error
    s = 'foo'
    n = alloc_nsstring(s)
    assert_raise(TypeError) { s * '' }
    assert_raise(TypeError) { n * '' }
  end
  
  def test_plus
    s = 'foo'
    n = alloc_nsstring(s)
    s = s + 'bar'
    n = n + 'bar'
    assert_equal(s, n)
  end
  
  def test_plus_error
    s = 'foo'
    n = alloc_nsstring(s)
    assert_raise(TypeError) { s + 42 }
    assert_raise(TypeError) { n + 42 }
    assert_raise(TypeError) { s + nil }
    assert_raise(TypeError) { n + nil }
  end
  
  def test_concat
    with_kcode('utf-8') do
      s = alloc_nsstring('foo')
      s << 'abc'
      assert_equal('fooabc', s)
      s << 123
      assert_equal('fooabc{', s)
      s.concat 0x3053
      assert_equal('fooabc{こ', s)
    end
  end
  
  def test_concat_error
    s = 'foo'
    n = alloc_nsstring(s)
    assert_raise(TypeError) { s << [] }
    assert_raise(TypeError) { n << [] }
    assert_raise(TypeError) { s << nil }
    assert_raise(TypeError) { n << nil }
  end
  
  def test_ref_nth
    s = 'abc'
    n = alloc_nsstring(s)
    [0, -1, -3, -4, -10, 2, 3, 10].each do |i|
      assert_equal(s[i], n[i])
    end
    
    with_kcode('utf-8') do
      s = alloc_nsstring('foo かきくけこ')
      assert_equal(0x3051, s[-2])
      assert_equal(0x3053, s[8])
      assert_equal(nil, s[10])
      assert_equal(nil, s[-10])
    end
  end
  
  def test_ref_substr
    with_kcode('utf-8') do
      s = alloc_nsstring('foo かきくけこ')
      assert_equal('きくけ', s['きくけ'])
      assert_equal('', s[''])
      assert_equal(nil, s['abc'])
    end
  end
  
  def test_ref_range
    s = 'abc'
    n = alloc_nsstring(s)
    [0..0, 0..2, 0..10, 2..10, -1..0, -2..2, -3..2, -4..2].each do |i|
      assert_equal(s[i], n[i])
    end
    
    with_kcode('utf-8') do
      s = alloc_nsstring('foo かきくけこ')
      assert_equal('foo', s[0..2])
      assert_equal('oo', s[1...3])
      assert_equal('くけこ', s[-3..8])
      assert_equal('けこ', s[7..10])
      assert_equal(nil, s[-10..-9])
      assert_equal('', s[5..4])
      assert_equal(nil, s[10..10])
      assert_equal(nil, s[10..-2])
    end
  end
  
  def test_ref_nth_len
    s = 'abc'
    n = alloc_nsstring(s)
    [[0,0], [0,2], [0,10], [2,10], [3,3], [-1,0], [-2,2], [-3,2], [-4,2]].each do |i|
      assert_equal(s[*i], n[*i])
    end
  end
  
  def test_ref_error
    n = alloc_nsstring('foo')
    assert_raise(TypeError) { n[[]] }
    assert_raise(TypeError) { n[{}] }
    assert_raise(TypeError) { n[nil] }
    assert_raise(TypeError) { n[3,nil] }
  end
  
  def test_assign_nth
    [[0,'AAA'], [2,''], [-1,'A'], [-3,'B']].each do |i,v|
      s = 'abc'
      n = alloc_nsstring(s)
      s[i] = v
      n[i] = v
      assert_equal(s, n)
    end
  end
  
  def test_assign_nth_error
    s = 'abc'
    n = alloc_nsstring(s)
    assert_raise(IndexError) { s[3] = '' }
    assert_raise(IndexError) { n[3] = '' }
    assert_raise(IndexError) { s[-4] = '' }
    assert_raise(IndexError) { n[-4] = '' }
    s = ''
    n = alloc_nsstring(s)
    assert_raise(IndexError) { s[0] = '' }
    assert_raise(IndexError) { n[0] = '' }
    assert_raise(IndexError) { s[-1] = '' }
    assert_raise(IndexError) { n[-1] = '' }
  end
  
  def test_assign_str
    [['','AAA'], ['a',''], ['c','ZZZ'], ['','']].each do |str,v|
      s = 'abc'
      n = alloc_nsstring(s)
      s[str] = v
      n[str] = v
      assert_equal(s, n)
    end
  end
  
  def test_assign_range
    [0..1, 1..2, 2..3, 3..6, -3..2, -3..-2, -1..-1, 3..2,
     0...2, 1...1, 1...2, 3...3, 3..3, -3...2, -3...-2, -1...-1, 3...2].each do |r|
      s = 'abc'
      n = alloc_nsstring(s)
      v = 'AAABBBCCC'
      s[r] = v
      n[r] = v
      assert_equal(s, n)
    end
  end
  
  def test_assign_range_error
    s = 'abc'
    n = alloc_nsstring(s)
    assert_raise(RangeError) { s[-10...3] = '' }
    assert_raise(RangeError) { n[-10...3] = '' }
    assert_raise(RangeError) { s[10..15] = '' }
    assert_raise(RangeError) { n[10..15] = '' }
  end
  
  def test_assign_nth_len
    [[0,1], [0,3], [0,10], [2,3], [3,5], [-2,1], [-3,1]].each do |i,len|
      s = 'abc'
      n = alloc_nsstring(s)
      v = 'ZZZZZZ'
      s[i,len] = v
      n[i,len] = v
      assert_equal(s, n)
    end
  end
  
  def test_assign_nth_len_error
    s = 'abc'
    n = alloc_nsstring(s)
    assert_raise(IndexError) { s[-10,3] = '' }
    assert_raise(IndexError) { n[-10,3] = '' }
    assert_raise(IndexError) { s[10,3] = '' }
    assert_raise(IndexError) { n[10,3] = '' }
    assert_raise(IndexError) { s[1,-3] = '' }
    assert_raise(IndexError) { n[1,-3] = '' }
  end
  
=begin
  def test_ref_regexp
    with_kcode('utf-8') do
      s = OSX::NSMutableString.stringWithString('foo かきくけこ')
      assert_equal('きくけ', s[/きくけ/])
      assert_equal('foo', s[/^foo/])
      assert_equal(nil, s[/ABC/])
    end
  end
  
  def test_ref_regexp_n
    with_kcode('utf-8') do
      s = OSX::NSMutableString.stringWithString('foo bar buz')
      assert_equal('foo bar', s[/([a-z]+) ([a-z]+)/,0])
      assert_equal('bar', s[/([a-z]+) ([a-z]+)/,2])
    end
  end
=end
  
  def test_capitalize
    ['foO bar buz', ''].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.capitalize, n.capitalize)
      assert_equal(s.capitalize!, n.capitalize!)
    end
  end
  
  def test_casecmp
    [['', ''], ['abc','abc'], ['AbC','abc'], ['012','abc']].each do |d|
      a, b = d
      c = alloc_nsstring(a)
      d = alloc_nsstring(b)
      assert_equal(a.casecmp(b), c.casecmp(d))
      assert_equal(b.casecmp(a), d.casecmp(c))
    end
  end
  
  def test_center
    [['abc',[6],' abc  '], ['',[3],'   '], ['abc',[1],'abc'],
     ['abc',[-6],'abc'], ['abc',[8,'012'],'01abc012'], ['abc',[12,'012'],'0120abc01201']].each do |d|
      s, param, res = d
      n = alloc_nsstring(s)
      r = alloc_nsstring(res)
      assert_equal(r, n.center(*param))
      assert_equal(s.center(*param), n.center(*param).to_s)
    end
    [['あいう',[6],' あいう  '], ['あいう',[12,'かき'],'かきかきあいうかきかきか']].each do |d|
      s, param, res = d
      n = alloc_nsstring(s)
      r = alloc_nsstring(res)
      assert_equal(r, n.center(*param))
    end
  end

  def test_chomp
    ["", "abc", "abc\n", "abc\r", "abc\r\n", "abc\n\n", "abc\r\n\r\n\r\n\n\n", "ab\nc\r"].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.chomp, n.chomp)
      assert_equal(s.chomp!, n.chomp!)
      n = alloc_nsstring(s)
      v = ''
      assert_equal(s.chomp(v), n.chomp(v))
      assert_equal(s.chomp!(v), n.chomp!(v))
      n = alloc_nsstring(s)
      v = "\r\n"
      assert_equal(s.chomp(v), n.chomp(v))
      assert_equal(s.chomp!(v), n.chomp!(v))
      n = alloc_nsstring(s)
      v = nil
      assert_equal(s.chomp(v), n.chomp(v))
      assert_equal(s.chomp!(v), n.chomp!(v))
    end
  end
  
  def test_chop
    ['abc', "abc\n", "abc\r\n", "abc\r", ''].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.chop, n.chop)
      assert_equal(s.chop!, n.chop!)
    end
  end
  
  def test_chop
    ['abc', "abc\n", "abc\r\n", "abc\r", ''].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.chop, n.chop)
      assert_equal(s.chop!, n.chop!)
    end
  end
  
  def test_chr
    n = alloc_nsstring('abc')
    assert_equal('a', n.chr)
    n = alloc_nsstring('')
    assert_equal('', n.chr)
  end
  
  def test_clear
    s = alloc_nsstring('Foobar')
    assert_equal(s, s.clear)
    assert_equal('', s)
    assert_equal(0, s.length)
  end
  
  def test_count
    [['a-z'], ['^a-z'], ['a-d','b-z'], ['0-9']].each do |d|
      s = 'Foobar Foobar'
      n = alloc_nsstring(s)
      assert_equal(s.count(*d), n.count(*d))
      assert_equal(s.count(*d.to_ns), n.count(*d.to_ns))
    end
  end
  
  def test_crypt
    s = 'abcdef'
    n = alloc_nsstring(s)
    salt = '.1/1'
    assert_equal(s.crypt(salt), n.crypt(salt))
  end
  
  def test_delete
    [['a-z'], ['^a-z'], ['a-d','b-z'], ['0-9']].each do |d|
      s = 'Foobar Foobar'
      n = alloc_nsstring(s)
      assert_equal(s.delete(*d), n.delete(*d))
      assert_equal(s.delete(*d.to_ns), n.delete(*d.to_ns))
      assert_equal(s.delete!(*d), n.delete!(*d))
      assert_equal(s, n)
    end
  end
  
  def test_downcase
    ['foO bAr BuZ Z', 'abc', ''].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.downcase, n.downcase)
      assert_equal(s.downcase!, n.downcase!)
    end
  end
  
  def test_dump
    ['', 'abc', "\r\n\v", 'あいう'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.dump, n.dump)
    end
  end
  
  def test_each_byte
    with_kcode('utf-8') do
      ['abc\r\ndef', 'a', 'あいうabc\r\nかきく', ''].each do |s|
        n = alloc_nsstring(s)
        a = []
        b = []
        n.each_byte {|i| a << i }
        s.each_byte {|i| b << i }
        assert_equal(b, a)
      end
    end
  end
  
  def test_each_line
    ["abc\ndef\r\nghi\njkl\n", "\n\n\nabc", "", "a\nb", "abc\rdef",
     "\nabc\n\ndef\nghi\njkl\n\nmnopq\n\n"].each do |s|
      n = alloc_nsstring(s)
      a = []
      b = []
      n.each_line {|i| a << i.to_ruby }
      s.each_line {|i| b << i }
      assert_equal(b, a)
      a = []
      b = []
      n.each_line(nil) {|i| a << i.to_ruby }
      s.each_line(nil) {|i| b << i }
      assert_equal(b, a)
      a = []
      b = []
      n.each_line("\r\n") {|i| a << i.to_ruby }
      s.each_line("\r\n") {|i| b << i }
      assert_equal(b, a)
      a = []
      b = []
      n.each_line('') {|i| a << i.to_ruby }
      s.each_line('') {|i| b << i }
      assert_equal(b, a)
    end
  end
  
  def test_empty
    s = alloc_nsstring('Foobar')
    assert_equal(false, s.empty?)
    assert_equal(true, s.clear.empty?)
  end
  
  def test_end_with
    s = alloc_nsstring('abc def')
    assert_equal(false, s.end_with?('abc'))
    assert_equal(true, s.end_with?('def'))
  end
  
  def test_gsub
    [[/a-z/,'+'], [/^a-c/,'---'], [/^A-Z/,''], [/0-9/,'']].each do |d|
      s = 'Foobar Fooooooobaaaaar'
      n = alloc_nsstring(s)
      assert_equal(s.gsub(*d), n.gsub(*d))
      assert_equal(s.gsub!(*d), n.gsub!(*d))
      assert_equal(s, n)

      s = 'Foobar Fooooooobaaaaar'
      n = alloc_nsstring(s)
      sa = []
      sr = s.gsub(d[0]) {|i| sa << i; d[1]}
      na = []
      nr = n.gsub(d[0]) {|i| na << i; d[1]}
      assert_equal(sa, na)
      assert_equal(sr, nr)
    end
  end
  
  def test_hex
    ['', '10', '-10', 'ff', '0x10', '-0x10', '0b10', 'xyz', '10z', '1_0'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.hex, n.hex)
    end
  end
  
  def test_include
    with_kcode('utf-8') do
      s = alloc_nsstring('abc def')
      assert_equal(true, s.include?('c d'))
      assert_equal(true, s.include?(0x62))
      assert_equal(false, s.include?('C D'))
      assert_equal(false, s.include?(0x41))
      s = alloc_nsstring('abc かきくけこ')
      assert_equal(true, s.include?('かき'))
      assert_equal(true, s.include?(0x3053))
      assert_equal(false, s.include?('は'))
      assert_equal(false, s.include?(0x3060))
    end
  end
  
  def test_include_error
    s = 'abc'
    n = alloc_nsstring(s)
    assert_raise(TypeError) { s.include?([]) }
    assert_raise(TypeError) { n.include?([]) }
  end
  
  def test_index
    ['', 'a', 'z', 0x42, 0x100000].each do |i|
      s = 'abc'
      n = alloc_nsstring(s)
      assert_equal(s.index(i), n.index(i))
    end
    [-10,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,10].each do |i|
      s = 'abcabc'
      n = alloc_nsstring(s)
      assert_equal(s.index('abc',i), n.index('abc',i))
    end
  end
  
  def test_index_error
    s = ''
    n = alloc_nsstring(s)
    assert_raise(TypeError) { s.index([]) }
    assert_raise(TypeError) { n.index([]) }
  end
  
  def test_insert
    [-4,-3,-2,-1,0,1,2,3].each do |i|
      s = 'abc'
      n = alloc_nsstring(s)
      assert_equal(s.insert(i,'ZZ'), n.insert(i,'ZZ'))
      assert_equal(s, n)
    end
  end
  
  def test_insert_error
    [-10-5,4,10].each do |i|
      s = 'abc'
      n = alloc_nsstring(s)
      assert_raise(IndexError) { s.insert(i,'ZZZ') }
      assert_raise(IndexError) { n.insert(i,'ZZZ') }
    end
    s = 'abc'
    n = alloc_nsstring(s)
    assert_raise(TypeError) { s.insert([], '') }
    assert_raise(TypeError) { n.insert([], '') }
    assert_raise(TypeError) { s.insert(0, 0) }
    assert_raise(TypeError) { n.insert(0, 0) }
  end
  
  def test_intern
    ['foo', 'abc_def', 'A_b_C'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.intern, n.intern)
      assert_equal(s.to_sym, n.to_sym)
    end
  end
  
  def test_lines
    ["abc\ndef\r\nghi\njkl\n", "\n\n\nabc", "", "a\nb", "abc\rdef",
     "\nabc\n\ndef\nghi\njkl\n\nmnopq\n\n"].each do |s|
      n = alloc_nsstring(s)
      a = []
      s.each {|i| a << i }
      assert_equal(n.lines, a)
    end
  end
  
  def test_ljust
    [['abc',[6],'abc   '], ['',[3],'   '], ['abc',[1],'abc'],
     ['abc',[-6],'abc'], ['abc',[8,'012'],'abc01201'], ['abc',[9,'012'],'abc012012']].each do |d|
      s, param, res = d
      n = alloc_nsstring(s)
      r = alloc_nsstring(res)
      assert_equal(r, n.ljust(*param))
      assert_equal(s.ljust(*param), n.ljust(*param).to_s)
    end
    [['あいう',[6],'あいう   '], ['あいう',[6,'かき'],'あいうかきか']].each do |d|
      s, param, res = d
      n = alloc_nsstring(s)
      r = alloc_nsstring(res)
      assert_equal(r, n.ljust(*param))
    end
  end
  
  def test_lstrip
    ["", "   abc   ", "abc", "\t \r\n\f\vtest \t"].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.lstrip, n.lstrip)
      assert_equal(s.lstrip!, n.lstrip!)
    end
  end
  
  def test_next
    ['aa', '99', 'a9', 'Aa', 'zz', '-9', '9', '09'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.next, n.next)
      assert_equal(s.succ, n.succ)
      assert_equal(s.next!, n.next!)
      assert_equal(s.succ!, n.succ!)
    end
  end
  
  def test_oct
    ['', '10', '-10', '010', '8', '0b10', '0x10', '1_0_0x'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.oct, n.oct)
    end
  end
  
  def test_ord
    ['', 'a', 'Z', '0', "\n"].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s[0] || 0, n.ord)
    end
  end
  
  def test_partition
    [['abcdefghicd','cd',['ab','cd','efghicd']], ['abc','x',['abc','','']]].each do |d|
      s, sep, res = d
      n = alloc_nsstring(s)
      assert_equal(res, n.partition(sep).to_ruby)
    end
  end
  
  def test_replace
    s = 'abc'
    n = alloc_nsstring(s)
    n.replace('ZZZ')
    assert_equal('ZZZ', n)
  end
  
  def test_reverse
    ['foO bar buZ', 'a', ''].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.reverse, n.reverse)
      assert_equal(s.reverse!, n.reverse!)
    end
    with_kcode('utf-8') do
      ['漢字', "あいうえおab\r\ncかきくけこ"].each do |s|
        n = alloc_nsstring(s)
        assert_equal(n, n.reverse.reverse)
      end
    end
  end
  
  def test_rindex
    ['', 'a', 'z', 0x42, 0x100000].each do |i|
      s = 'abcabc'
      n = alloc_nsstring(s)
      assert_equal(s.rindex(i), n.rindex(i))
    end
    [-10,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,10].each do |i|
      s = 'abcabc'
      n = alloc_nsstring(s)
      assert_equal(s.rindex('abc',i), n.rindex('abc',i))
    end
  end
  
  def test_rjust
    [['abc',[6],'   abc'], ['',[3],'   '], ['abc',[1],'abc'],
     ['abc',[-6],'abc'], ['abc',[8,'012'],'01201abc'], ['abc',[9,'012'],'012012abc']].each do |d|
      s, param, res = d
      n = alloc_nsstring(s)
      r = alloc_nsstring(res)
      assert_equal(r, n.rjust(*param))
      assert_equal(s.rjust(*param), n.rjust(*param).to_s)
    end
    [['あいう',[6],'   あいう'], ['あいう',[6,'かき'],'かきかあいう']].each do |d|
      s, param, res = d
      n = alloc_nsstring(s)
      r = alloc_nsstring(res)
      assert_equal(r, n.rjust(*param))
    end
  end
  
  def test_rpartition
    [['abcdefghicd','cd',['abcdefghi','cd','']], ['abc','x',['abc','','']]].each do |d|
      s, sep, res = d
      n = alloc_nsstring(s)
      assert_equal(res, n.rpartition(sep).to_ruby)
    end
  end
  
  def test_rstrip
    ["", "   abc   ", "abc", "\t \r\n\f\vtest \t \r\n\f\v"].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.rstrip, n.rstrip)
      assert_equal(s.rstrip!, n.rstrip!)
    end
  end
  
  def test_scan
    s = "aDb0c 0d3def"
    n = s.to_ns
    re = /..[a-z]?/
    assert_equal(s.scan(re), n.scan(re))
    
    s = "ab0c 0dAdef"
    n = s.to_ns
    re = /..[a-z]?/
    sa = []
    sr = s.scan(re) {|i| sa << i}
    na = []
    nr = n.scan(re) {|i| na << i}
    assert_equal(sa, na)
    assert_equal(sr, nr)
  end
  
  def test_size
    assert_equal(6, 'Foobar'.to_ns.size)
  end
  
  def test_slice
    ['abc def'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.slice(1), n.slice(1))
      assert_equal(s.slice(3,2), n.slice(3,2))
      assert_equal(s.slice(4..6), n.slice(4..6))
      assert_equal(s.slice(4...6), n.slice(4...6))
    end
  end
  
  def test_slice!
    [-2, 0, 2, 6, 10].each do |i|
      s = 'abc def'
      n = alloc_nsstring(s)
      assert_equal(s.slice!(i), n.slice!(i))
      assert_equal(s, n)
    end
    [[-2,2], [0,2], [3,4], [4,6], [6,-2]].each do |i|
      s = 'abc def'
      n = alloc_nsstring(s)
      assert_equal(s.slice!(*i), n.slice!(*i))
      assert_equal(s, n)
    end
    ['', 'bc', 'def', 'x'].each do |i|
      s = 'abc def'
      n = alloc_nsstring(s)
      assert_equal(s.slice!(i), n.slice!(i))
      assert_equal(s, n)
    end
    [-2..2, 0...0, 1..2, 4...10, 10...20].each do |i|
      s = 'abc def'
      n = alloc_nsstring(s)
      assert_equal(s.slice!(i), n.slice!(i))
      assert_equal(s, n)
    end
    
    with_kcode('utf-8') do
    end
  end
  
  def test_split
    (-1).upto(10) do |d|
      s = ',,a,b,,c,d,e,,'
      sep = ','
      limit = d
      n = alloc_nsstring(s)
      assert_equal(s.split(sep, limit), n.split(sep, limit))
    end
    [-1,0,8,50].each do |d|
      s = '   ,,, ,,,aaa,bbb,c,,d,,,  ,,,    '
      sep = ','
      limit = d
      n = alloc_nsstring(s)
      assert_equal(s.split(sep, limit), n.split(sep, limit))
    end
    [-1,0,3,30].each do |d|
      s = '      aaa  bbb c    d      '
      sep = ' '
      limit = d
      n = alloc_nsstring(s)
      assert_equal(s.split(sep, limit), n.split(sep, limit))
    end
    [-1,0,3,8,30].each do |d|
      s = '  a b c  '
      sep = nil
      limit = d
      n = alloc_nsstring(s)
      assert_equal(s.split(sep, limit), n.split(sep, limit))
    end
  end
  
  def test_squeeze
    [['a-z'], ['^a-c'], ['^A-Z','a'], ['0-9']].each do |d|
      s = 'Foobar Fooooooobaaaaar'
      n = alloc_nsstring(s)
      assert_equal(s.squeeze(*d), n.squeeze(*d))
      assert_equal(s.squeeze!(*d), n.squeeze!(*d))
      assert_equal(s, n)
    end
  end
  
  def test_start_with
    s = alloc_nsstring('abc def')
    assert_equal(true, s.start_with?('abc'))
    assert_equal(false, s.start_with?('def'))
  end
  
  def test_strip
    ["", "   abc   ", "\t \r\n\f\vtest \t"].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.strip, n.strip)
      assert_equal(s.strip!, n.strip!)
    end
  end
  
  def test_sub
    [[/[a-z]+/,'+'], [/[^a-c]+/,'---'], [/[^A-Z]+/,''], [/[0-9]/,'']].each do |d|
      s = 'Foobar Fooooooobaaaaar'
      n = alloc_nsstring(s)
      assert_equal(s.sub(*d), n.sub(*d))
      assert_equal(s.sub!(*d), n.sub!(*d))
      assert_equal(s, n)

      s = 'Foobar Fooooooobaaaaar'
      n = alloc_nsstring(s)
      sa = []
      sr = s.sub(d[0]) {|i| sa << i; d[1]}
      na = []
      nr = n.sub(d[0]) {|i| na << i; d[1]}
      assert_equal(sa, na)
      assert_equal(sr, nr)
    end
  end
  
  def test_sum
    ['', 'azxcvzxcvrqfsdfiaosjdoj2980qurt092g09jrjbhq02jgh0'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.sum, n.sum)
      assert_equal(s.sum(8), n.sum(8))
      assert_equal(s.sum(11), n.sum(11))
    end
    n = alloc_nsstring('あいうえおかきくけこ')
    assert_equal(58089, n.sum)
    assert_equal(233, n.sum(8))
  end
  
  def test_swapcase
    ['', 'aBc', 'AbC ＡＢｃ', '012'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.swapcase, n.swapcase)
      assert_equal(s.swapcase!, n.swapcase!)
    end
  end
  
  def test_tr
    [['a-z','A-Z'], ['^A-Z','^a-z'], ['0-9','a-j'], ['z','Z']].each do |d|
      s = 'Foobar Fooooooobaaaaar 0001239543333'
      n = alloc_nsstring(s)
      assert_equal(s.tr(*d), n.tr(*d))
      assert_equal(s.tr(*d.to_ns), n.tr(*d.to_ns))
      assert_equal(s.tr!(*d), n.tr!(*d))
      assert_equal(s, n)
    end
  end
  
  def test_tr_s
    [['a-z','A-Z'], ['^A-Z','^a-z'], ['0-9','a-j'], ['z','Z']].each do |d|
      s = 'Foobar Fooooooobaaaaar 0001239543333'
      n = alloc_nsstring(s)
      assert_equal(s.tr_s(*d), n.tr_s(*d))
      assert_equal(s.tr_s(*d.to_ns), n.tr_s(*d.to_ns))
      assert_equal(s.tr_s!(*d), n.tr_s!(*d))
      assert_equal(s, n)
    end
  end
  
  def test_upcase
    ['foO bar buz', ''].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.upcase, n.upcase)
      assert_equal(s.upcase!, n.upcase!)
    end
  end
  
  def test_upto
    a = alloc_nsstring('aa')
    b = alloc_nsstring('ba')
    r = []
    a.upto(b) {|i| r << i }
    r.map! {|i| i.to_ruby }
    c = 'aa'
    d = 'ba'
    s = []
    c.upto(d) {|i| s << i }
    assert_equal(s, r)
  end
  
  def test_to_f
    ['', '3358.123', '4_42.42', '1e-5', '12e5', '.1', "  \n10.12"].each do |s|
      n = alloc_nsstring(s)
      assert((s.to_f - n.to_f).abs < 0.01)
    end
  end
  
  def test_to_i
    ['', '-12345', '42', '3358.123', '1_000_1', '0x10'].each do |s|
      n = alloc_nsstring(s)
      assert_equal(s.to_i, n.to_i)
    end
  end
  
  def test_method_missing_dock_typing
    assert(''.to_ns.ducktype_test)
    assert_nothing_raised { ''.to_ns.ducktype_test }
  end
  
  def test_warning_methods
    #'a'.to_ns.match(/[a-z]/)
    #'abcdef'.to_ns =~(/([a-z]+)/)
  end
end


class String
  def ducktype_test
    true
  end
end
