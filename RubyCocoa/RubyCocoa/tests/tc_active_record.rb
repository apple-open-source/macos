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
  require 'sqlite3'
  
  dbfile = '/tmp/maildemo.sqlite'
  File.delete(dbfile) if File.exist?(dbfile)
  system("sqlite3 #{dbfile} < #{ File.join(File.dirname( File.expand_path(__FILE__) ), 'maildemo.sql') }")

  ActiveRecord::Base.establish_connection({
    :adapter => 'sqlite3',
    :dbfile => dbfile
  })

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

  class FakePointer
    attr_reader :value
    def initialize; @value = nil; end
    def assign(value); @value = value; end
  end

  class TC_ActiveRecord < Test::Unit::TestCase
  
    def setup
      # when we need more complex tests here should probably go some code that flushes the db
    end
    
    # ---------------------------------------------------------
    # Class additions
    # ---------------------------------------------------------
    
    # Array
    def test_array_of_activerecords_to_proxies
      assert Mailbox.find(:all).to_activerecord_proxies.first.is_a?(MailboxProxy)
    end
    def test_array_of_proxies_to_original_activerecords
      mailboxes = Mailbox.find(:all).to_activerecord_proxies
      assert mailboxes.original_records.first.is_a?(Mailbox)
    end
    # ActiveRecord::Base
    def test_automatically_creates_a_proxy
      assert Object.const_defined?('MailboxProxy')
    end
    def test_activerecord_to_proxy
      mailbox = Mailbox.new({'title' => 'foo'})
      mailbox.save
    
      proxy = mailbox.to_activerecord_proxy
      assert proxy.is_a?(MailboxProxy)
      assert_equal mailbox, proxy.to_activerecord
    end
    
    # ---------------------------------------------------------
    # Subclasses of cocoa classes that add ActiveRecord support
    # ---------------------------------------------------------
    
    # ActiveRecordTableView
    def test_column_scaffolding
      tableview = OSX::ActiveRecordTableView.alloc.init
      recordset_controller = OSX::ActiveRecordSetController.alloc.init
      
      # make sure that the required args are passed
      assert_raises ArgumentError do
        tableview.scaffold_columns_for(:model => nil, :bind_to => recordset_controller)
      end
      assert_raises ArgumentError do
        tableview.scaffold_columns_for(:model => Email, :bind_to => nil)
      end
      
      # make sure the scaffolding will only contains the columns for: address, subject, body
      tableview.scaffold_columns_for :model => Email, :bind_to => recordset_controller, :except => ['id', 'updated_at', 'mailbox_id']
      assert tableview.tableColumns.map { |column| column.identifier.to_s } == ['address', 'subject', 'body']
      
      # test block
      # FIXME: I want to be able to test if some bindings options have been set,
      # unfortunately I can't find a way to get the bindings options back from the table column.
      tableview.scaffold_columns_for :model => Email, :bind_to => recordset_controller, :except => 'mailbox_id' do |column, column_options|
        if column.identifier.to_s == 'address'
          column.headerCell.setStringValue 'foo'
        end
      end
      tableview.tableColumns.each do |column|
        if column.identifier.to_s == 'address'
          assert column.headerCell.stringValue.to_s == 'foo'
        else
          assert column.headerCell.stringValue.to_s.downcase.gsub(/\s/, '_') == column.identifier.to_s
        end
      end
    end
    
    
    # ActiveRecordProxy
    def test_proxy_init
      before = Mailbox.count
      proxy  = MailboxProxy.alloc.init
      assert Mailbox.count == (before + 1)
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
      before = Mailbox.count
      proxy = MailboxProxy.alloc.initWithAttributes({ 'title' => 'initWithAttributes' })
      assert Mailbox.count == (before + 1)
      assert proxy.to_activerecord.title == 'initWithAttributes'
    end
    
    def test_generated_instance_methods_on_proxy
      mailbox = Mailbox.find(:first).to_activerecord_proxy
      # assign through generated setter
      mailbox.title = 'foo'
      assert mailbox['title'] == 'foo'
      # check through generated getter
      mailbox['title'] = 'bar'
      assert mailbox.title == 'bar'
      
      # make sure that others don't work
      assert_raises OSX::OCMessageSendException do
        mailbox.pretty_sure_this_one_does_not_exist = 'foo'
      end
    end
    
    def test_methods_from_record
      mailbox = Mailbox.find(:first)
      proxy = mailbox.to_activerecord_proxy
      assert proxy.record_methods == mailbox.methods
    end
  
    def test_proxy_is_association?
      proxy = Mailbox.find(:first).to_activerecord_proxy
      assert proxy.is_association?('emails')
      assert !proxy.is_association?('title')
    end
  
    def test_proxy_method_forwarding
      mailbox = Mailbox.find(:first).to_activerecord_proxy
      assert Mailbox.find(:first).title == mailbox.title
    end
  
    def test_proxy_set_and_get_value_for_key
      mailbox = MailboxProxy.alloc.initWithAttributes({'title' => 'bla'})
      mailbox.setValue_forKey( [EmailProxy.alloc.initWithAttributes({'address' => 'bla@example.com', 'subject' => nil, 'body' => 'foobar'})], 'emails' )
  
      assert mailbox.valueForKey('title').to_s == 'bla'
      assert mailbox.valueForKey('emails')[0].valueForKey('body').string.to_s == 'foobar'
    
      # check the ability to override the valueForKey method in a subclass
      #
      # block
      assert mailbox.valueForKey('emails')[0].valueForKey('body').is_a?(OSX::NSAttributedString)
      # return
      assert mailbox.valueForKey('emails')[0].valueForKey('subject').is_a?(OSX::NSAttributedString)
      assert mailbox.valueForKey('emails')[0].valueForKey('subject').string.to_s == '' # check that we get a new instace, because the value was set to nil
      # call
      assert mailbox.valueForKey('emails')[0].valueForKey('address').is_a?(OSX::NSAttributedString)
    end
  
    def test_proxy_validate_value_for_key_with_error
      mailbox = Mailbox.find(:first).to_activerecord_proxy
      before = mailbox.title
      pointer = FakePointer.new
      result = mailbox.validateValue_forKeyPath_error([''], 'title', pointer)
    
      assert result == false
      assert mailbox.title == before
      assert pointer.value.is_a?(OSX::NSError)
      assert pointer.value.userInfo[OSX::NSLocalizedDescriptionKey].to_s == "Mailbox title can't be blank\n"
    end
  
    # ActiveRecordProxy class methods
    def test_proxy_to_model_class
      assert MailboxProxy.model_class == Mailbox
    end
    
    def test_find_first
      mailbox = Mailbox.find(:first)
      proxy = MailboxProxy.find(:first)
      assert mailbox == proxy.original_record
    end
    
    def test_find_all
      mailboxes = Mailbox.find(:all)
      proxies = MailboxProxy.find(:all)
      assert mailboxes.last == proxies.last.original_record
    end
    
    def test_find_by
      mailbox = Mailbox.find_by_title('foo')
      proxy = MailboxProxy.find_by_title('foo')
      assert mailbox == proxy.original_record
    end
  end

rescue LoadError
  $stderr.puts 'Skipping osx/active_record tests, you need to have active_record and sqlite3'
end
