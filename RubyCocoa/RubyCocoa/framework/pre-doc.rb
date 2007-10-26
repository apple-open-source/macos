require 'fileutils'

unless File.exist?('bridge-doc')
  begin
    Dir.chdir('tool') do
      ruby("gen_bridge_doc.rb build ../bridge-doc")
    end
  rescue RuntimeError => e
    rm_rf 'bridge-doc'
    raise e
  end
end
