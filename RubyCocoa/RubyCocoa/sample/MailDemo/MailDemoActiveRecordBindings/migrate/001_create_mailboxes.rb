class CreateMailboxes < ActiveRecord::Migration
  def self.up
    create_table :mailboxes do |t|
      t.column :title, :string, :default => 'title'
    end
  end

  def self.down
    drop_table :mailboxes
  end
end