#
#  $Id: tc_kvc_kvo.rb 2212 2008-07-02 17:12:47Z kimuraw $
#
#  Copyright (c) 2008 kimura wataru
#

require 'test/unit'
require 'osx/cocoa'

class KVOneToOne < OSX::NSObject
  attr_accessor :value1, :value2
  attr_reader :observed
  kvc_accessor :kvc1, :kvc2

  def initialize
    @observed = Hash.new(0)
  end

  def self.automaticallyNotifiesObserversForKey(key)
    case key.to_s 
    when 'value1'
      true
    when 'kvc1'
      true
    else
      false
    end
  end

  ## observer
  def observeValueForKeyPath_ofObject_change_context(path, obj, change, context)
    @observed[path.to_s] += 1
  end
end

class KVToMany < OSX::NSObject
  kvc_array_accessor :key1, :key2
  attr_reader :observed

  def initialize
    @observed = []
    @key1 = @key2 = []
  end
  
  def self.automaticallyNotifiesObserversForKey(key)
    case key.to_s 
    when 'key1'
      true
    else
      false
    end
  end

  ## observer
  KVONotifInfo = Struct.new(:path, :change, :indexes)
  def observeValueForKeyPath_ofObject_change_context(path, obj, change, context)
    info = KVONotifInfo.new(path.to_s, change[OSX::NSKeyValueChangeKindKey], change[OSX::NSKeyValueChangeIndexesKey])
    @observed.push info
  end
end

class KVCMultiInvoked < OSX::NSObject
  kvc_accessor :key1, :key2 # 1st
  kvc_accessor :key1, :key2 # 2nd
end

class TC_KeyValueObserving < Test::Unit::TestCase

  def test_rbobject_kvo_autonotify
    obj = KVOneToOne.alloc.init
    [:value1, :value2, :kvc1, :kvc2].each do |key|
      obj.addObserver_forKeyPath_options_context(obj, key,
        OSX::NSKeyValueObservingOptionNew | OSX::NSKeyValueObservingOptionOld, nil)
    end
    # ruby accessor
    obj.setValue_forKey(1, :value1)
    obj.setValue_forKey(2, :value2)
    assert_equal(1, obj.observed['value1'], "observed count of value1")
    assert_equal(0, obj.observed['value2'], "observed count of value1")
    # accessor defined by kvc_writer
    obj.setValue_forKey(1, :kvc1)
    obj.setValue_forKey(2, :kvc2)
    assert_equal(1, obj.observed['kvc1'], "observed count of kvc1")
    assert_equal(0, obj.observed['kvc2'], "observed count of kvc2")
    obj.kvc1 = 3
    obj.kvc2 = 4
    assert_equal(2, obj.observed['kvc1'], "observed count of kvc1")
    assert_equal(0, obj.observed['kvc2'], "observed count of kvc2")
    #
    [:value1, :value2, :kvc1, :kvc2].each do |key|
      obj.removeObserver_forKeyPath(obj, key)
    end
  end

  def test_rbobject_kvo_tomany_autonotify
    obj = KVToMany.alloc.init
    [:key1, :key2].each do |key|
      obj.addObserver_forKeyPath_options_context(obj, key,
        OSX::NSKeyValueObservingOptionNew | OSX::NSKeyValueObservingOptionOld, nil)
    end
    # auto notification
    ## insert
    obj.insertObject_inKey1AtIndex(1, 0)
    obs = obj.observed.pop
    assert_equal('key1', obs.path)
    assert_equal(OSX::NSKeyValueChangeInsertion, obs.change)
    ## replace
    obj.replaceObjectInKey1AtIndex_withObject(0, 2)
    obs = obj.observed.pop
    assert_equal('key1', obs.path)
    assert_equal(OSX::NSKeyValueChangeReplacement, obs.change)
    ## remove
    obj.removeObjectFromKey1AtIndex(0)
    obs = obj.observed.pop
    assert_equal('key1', obs.path)
    assert_equal(OSX::NSKeyValueChangeRemoval, obs.change)
    # disable auto notification
    ## insert
    obj.insertObject_inKey2AtIndex(1, 0)
    assert_equal(0, obj.observed.size)
    ## replace
    obj.replaceObjectInKey2AtIndex_withObject(0, 2)
    assert_equal(0, obj.observed.size)
    ## remove
    obj.removeObjectFromKey2AtIndex(0)
    assert_equal(0, obj.observed.size)
    #
    [:key1, :key2].each do |key|
      obj.removeObserver_forKeyPath(obj, key)
    end
  end

  def test_kvc_multi_invoked
    obj = KVCMultiInvoked.alloc.init
    assert_nothing_thrown {obj.key1 = 300}
    assert_equal(300, obj.key1)
  end

end

