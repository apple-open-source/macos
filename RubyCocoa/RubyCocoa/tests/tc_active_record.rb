#
#  Copyright (c) 2007 Eloy Duran <e.duran@superalloy.nl>
#

require 'test/unit'
begin
  begin
    require 'rubygems'
  rescue LoadError
  end
  require 'osx/active_record'
  #require File.expand_path('../../framework/src/ruby/osx/objc/active_record', __FILE__)
  require 'sqlite3'
  
  # FIXME: Why is this necessary in the latest trunk? (r2231)
  # def OSX._ignore_ns_override
  #   true
  # end
  
  class Object
    def __stub(mname, *return_values)
      @return_values = return_values
      instance_eval %{
        def self.#{mname}
          return *@return_values
        end
      }
    end
  end
  
  class FakePointer
    attr_reader :value
    def initialize; @value = nil; end
    def assign(value); @value = value; end
  end
  
  ActiveRecord::Base.establish_connection({
    :adapter => 'sqlite3',
    :dbfile => ':memory:'
  })
  ActiveRecord::Migration.verbose = false
  
  module ARTestHelper
    def setup_db
      ActiveRecord::Schema.define do
        create_table :mailboxes do |t|
          t.column :title, :string, :default => 'title'
        end

        create_table :emails do |t|
          t.column :mailbox_id, :integer
          t.column :address, :string, :default => "test@test.com"
          t.column :subject, :string, :default => "test subject"
          t.column :body, :text
          t.column :updated_at, :datetime
        end
      end
    end
    
    def teardown_db
      ActiveRecord::Base.connection.tables.each do |table|
        ActiveRecord::Base.connection.drop_table(table)
      end
    end
    
    def setup; setup_db; end
    def teardown; teardown_db; end
    
    def assert_difference(eval_string, difference)
      initial_value = eval(eval_string)
      yield
      assert_equal (initial_value + difference), eval(eval_string)
    end
    
    def assert_no_difference(eval_string)
      initial_value = eval(eval_string)
      yield
      assert_equal initial_value, eval(eval_string)
    end
  end
  
  class Mailbox < ActiveRecord::Base
    
    has_many :emails
    
    validates_presence_of :title
  end
  
  # Commented this out so we can test the automatic generation of proxy classes
  #
  # class MailboxProxy < OSX::ActiveRecordProxy
  # end
  
  class Email < ActiveRecord::Base
    
    belongs_to :mailbox
  end
  
  class EmailProxy < OSX::ActiveRecordProxy
    # on_get filter with: block
    on_get :body do |content|
      content ||= 'empty'
      OSX::NSAttributedString.alloc.initWithString(content)
    end
    
    # on_get filter with: return
    on_get :subject, :return => [OSX::NSAttributedString, :initWithString]
    
    # on_get filter with: call
    on_get :address, :call => :nsattributed_string_from_address
    # and the method to be called
    def nsattributed_string_from_address(address)
      address ||= 'empty'
      OSX::NSAttributedString.alloc.initWithString(address)
    end
  end

  class TC_ActiveRecordClassExtensions < Test::Unit::TestCase
    include ARTestHelper
    
    def setup
      setup_db
      @mailbox = Mailbox.create({'title' => 'foo'})
    end
    
    # Array
    
    def test_array_of_activerecords_to_proxies
      assert_kind_of MailboxProxy, Mailbox.find(:all).to_activerecord_proxies.first
    end
    
    def test_array_of_proxies_to_original_activerecords
      assert_kind_of Mailbox, Mailbox.find(:all).to_activerecord_proxies.original_records.first
    end
    
    # ActiveRecord::Base
    
    def test_automatically_creates_a_proxy
      assert Object.const_defined?('MailboxProxy')
    end
    
    def test_activerecord_to_proxy
      proxy = Mailbox.find(:first).to_activerecord_proxy
      assert_kind_of MailboxProxy, proxy
      assert_equal @mailbox, proxy.to_activerecord
    end
    
    def test_namespaced_activerecord_base_subclassing
      assert_nothing_raised NameError do
        eval "module TestNamespace; class BaseSubclass < ActiveRecord::Base; end; end"
      end
    end
  end
  
  class TC_ActiveRecordSetController < Test::Unit::TestCase
    include ARTestHelper
    
    def setup
      setup_db
      3.times { Mailbox.create }
      @controller = OSX::ActiveRecordSetController.alloc.init
      @controller.objectClass = MailboxProxy
      @controller.content = MailboxProxy.find(:all)
    end
    
    def test_should_instantiate_and_save_a_record_on_newObject
      assert_difference('Mailbox.count', +1) do
        @controller.newObject
      end
    end
    
    def test_should_destroy_a_record_on_remove
      @controller.__stub(:selectedObjects, [MailboxProxy.find(:first)].to_ns)
      assert_difference('Mailbox.count', -1) do
        @controller.remove(nil)
      end
    end
    
    def test_should_destroy_multiple_records_on_remove
      @controller.__stub(:selectedObjects, MailboxProxy.find(:all, :limit => 2).to_ns)
      assert_difference('Mailbox.count', -2) do
        @controller.remove(nil)
      end
    end
  end

  class TC_BelongsToActiveRecordSetController < Test::Unit::TestCase
    include ARTestHelper
    
    def setup
      setup_db
      @controller = OSX::BelongsToActiveRecordSetController.alloc.init
      @controller.objectClass = MailboxProxy
      @controller.content = MailboxProxy.find(:all)
    end
    
    def test_should_instantiate_but_not_save_a_record_on_newObject
      assert_no_difference('Mailbox.count') do
        @controller.newObject
      end
    end
  end
  
  class TC_ActiveRecordTableView < Test::Unit::TestCase
    include ARTestHelper
    
    def setup
      setup_db
      @tableview = OSX::ActiveRecordTableView.alloc.init
      @recordset_controller = OSX::ActiveRecordSetController.alloc.init
    end
    
    def test_should_raise_argument_errors_for_missing_arguments_when_trying_to_scaffold
      assert_raises ArgumentError do
        @tableview.scaffold_columns_for :model => nil, :bind_to => @recordset_controller
      end
      assert_raises ArgumentError do
        @tableview.scaffold_columns_for :model => Email, :bind_to => nil
      end
    end
    
    # FIXME: I don't want to be the one to cause a segfault in the tests,
    # so for now these are commented, but they should work under normal circumstances.
    #
    # def test_should_scaffold_columns_without_block_and_without_specific_fields
    #   # make sure the scaffolding will only contain the columns for: address, subject, body
    #   @tableview.scaffold_columns_for :model => Email, :bind_to => @recordset_controller, :except => ['id', 'updated_at', 'mailbox_id']
    #   assert_equal ['address', 'subject', 'body'], @tableview.tableColumns.map { |column| column.identifier.to_s }
    # end
    # 
    # def test_should_scaffold_columns_with_block_and_do_custom_stuff
    #   @tableview.scaffold_columns_for(:model => Email, :bind_to => @recordset_controller, :except => 'mailbox_id') do |column, column_options|
    #     column.headerCell.stringValue = 'foo' if column.identifier.to_s == 'address'
    #   end
    #   
    #   @tableview.tableColumns.each do |column|
    #     if column.identifier.to_s == 'address'
    #       assert_equal 'foo', column.headerCell.stringValue
    #     else
    #       assert_equal column.identifier.to_s, column.headerCell.stringValue.to_s.downcase.gsub(/\s/, '_')
    #     end
    #   end
    # end
  end
  
  class TC_ActiveRecordProxy < Test::Unit::TestCase
    include ARTestHelper
    
    def setup
      setup_db
      mailbox = Mailbox.create('title' => 'foo')
      mailbox.emails << Email.new
      @email_proxy = EmailProxy.find(:first)
    end
    
    def teardown
      teardown_db
    end
    
    def test_proxy_init_should_not_create_a_new_record
      assert_no_difference("Mailbox.count") do
        MailboxProxy.alloc.init
      end
    end
    
    def test_proxy_initWithRecord
      mailbox = Mailbox.new({ 'title' => 'initWithRecord' })
      mailbox.save
      
      before = Mailbox.count
      proxy = MailboxProxy.alloc.initWithRecord(mailbox)
      
      assert Mailbox.count == before
      assert_equal proxy.to_activerecord, mailbox
    end
    
    def test_proxy_initWithAttributes
      assert_difference('Mailbox.count', +1) do
        proxy = MailboxProxy.alloc.initWithAttributes({ 'title' => 'initWithAttributes' })
        assert_equal 'initWithAttributes', proxy.to_activerecord.title
      end
    end
    
    def test_should_always_return_the_same_cached_proxy_for_a_record_object
      proxy1 = @email_proxy.mailbox.to_activerecord_proxy
      proxy2 = @email_proxy.mailbox.to_activerecord_proxy
      assert_equal proxy1.object_id, proxy2.object_id
    end
    
    def test_should_only_define_all_the_proxy_methods_once
      temp_class_eval_email_proxy_class do
        assert_difference("EmailProxy.instance_variable_get(:@test_counter)", +1) do
          2.times { EmailProxy.find(:first) }
        end
      end
    end
    
    def test_should_return_a_proxy_when_requesting_a_belongs_to_associated_record
      assert_kind_of OSX::ActiveRecordProxy, @email_proxy.mailbox
    end
    
    def test_should_be_able_to_compare_proxies
      id = Mailbox.create(:title => 'compare').id
      m1 = MailboxProxy.find(id)
      m2 = MailboxProxy.find(id)
      assert_equal m1, m2
      assert_not_equal m1, 'foo'
    end
    
    def test_generated_instance_methods_on_proxy
      mailbox = MailboxProxy.find(:first)
      
      # assign through generated setter
      mailbox.title = 'baz'
      assert_equal 'baz', mailbox['title']
      
      # check through generated getter
      mailbox['title'] = 'bar'
      assert_equal 'bar', mailbox.title
      
      # make sure that others don't work
      assert_raises OSX::OCMessageSendException do
        mailbox.pretty_sure_this_one_does_not_exist = 'foo'
      end
    end
    
    def test_methods_from_record
      mailbox = Mailbox.find(:first)
      proxy = mailbox.to_activerecord_proxy
      assert_equal mailbox.methods, proxy.record_methods
    end
  
    def test_proxy_is_association?
      proxy = Mailbox.find(:first).to_activerecord_proxy
      assert proxy.is_association?('emails')
      assert !proxy.is_association?('title')
    end
  
    def test_proxy_method_forwarding
      mailbox = MailboxProxy.find(:first)
      assert_equal mailbox.title, Mailbox.find(:first).title
    end
  
    def test_proxy_set_and_get_value_for_key
      mailbox = MailboxProxy.alloc.initWithAttributes({'title' => 'bla'})
      mailbox.setValue_forKey( [EmailProxy.alloc.initWithAttributes({'address' => 'bla@example.com', 'subject' => nil, 'body' => 'foobar'})], 'emails' )
  
      assert_equal 'bla', mailbox.valueForKey('title')
      assert_equal 'foobar', mailbox.valueForKey('emails')[0].valueForKey('body').string
    
      # check the ability to override the valueForKey method in a subclass
      #
      # block
      assert_kind_of OSX::NSAttributedString, mailbox.valueForKey('emails')[0].valueForKey('body')
      
      # return
      assert_kind_of OSX::NSAttributedString, mailbox.valueForKey('emails')[0].valueForKey('subject')
      
      # check that we get a new instace, because the value was set to nil
      assert_equal '', mailbox.valueForKey('emails')[0].valueForKey('subject').string
      
      # call
      assert_kind_of OSX::NSAttributedString, mailbox.valueForKey('emails')[0].valueForKey('address')
    end
  
    def test_proxy_validate_value_for_key_with_error
      mailbox = Mailbox.find(:first).to_activerecord_proxy
      before = mailbox.title
      pointer = FakePointer.new
      result = mailbox.validateValue_forKeyPath_error([''], 'title', pointer)
    
      assert result == false
      assert_equal before, mailbox.title
      assert_kind_of OSX::NSError, pointer.value
      assert_equal "Mailbox title can't be blank\n", pointer.value.userInfo[OSX::NSLocalizedDescriptionKey]
    end
  
    # ActiveRecordProxy class methods
    def test_proxy_to_model_class
      assert_equal Mailbox, MailboxProxy.model_class
    end
    
    def test_find_first
      mailbox = Mailbox.find(:first)
      proxy = MailboxProxy.find(:first)
      assert_equal mailbox, proxy.original_record
    end
    
    def test_find_all
      mailboxes = Mailbox.find(:all)
      proxies = MailboxProxy.find(:all)
      assert_equal mailboxes, proxies.original_records
    end
    
    def test_find_by
      mailbox = Mailbox.find_by_title('foo')
      proxy = MailboxProxy.find_by_title('foo')
      assert_equal mailbox, proxy.original_record
    end
    
    private
    
    # belongs to test_should_only_define_all_the_proxy_methods_once
    def temp_class_eval_email_proxy_class
      EmailProxy.class_eval do
        @record_methods_defined = nil
        @test_counter = 0
        
        alias_method :original_define_record_methods!, :define_record_methods!
        def define_record_methods!
          original_define_record_methods!
          self.class.instance_variable_set(:@test_counter, self.class.instance_variable_get(:@test_counter) + 1)
        end
        
        yield
        
        alias_method :define_record_methods!, :original_define_record_methods!
      end
    end
  end

rescue LoadError
  $stderr.puts 'Skipping osx/active_record tests, you need to have active_record and sqlite3'
end
