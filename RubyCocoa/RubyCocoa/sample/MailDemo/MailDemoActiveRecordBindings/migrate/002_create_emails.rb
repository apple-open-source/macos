class CreateEmails < ActiveRecord::Migration
  def self.up
    create_table :emails do |t|
      t.column :address, :string, :default => 'test@test.com'
      t.column :subject, :string, :default => 'test subject'
      t.column :body, :text
      t.column :updated_at, :datetime
      t.column :mailbox_id, :integer
    end
  end

  def self.down
    drop_table :emails
  end
end