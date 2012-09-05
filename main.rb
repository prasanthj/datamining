require "rubygems"
require "bundler/setup"

require "nokogiri"

data_dir = "data"
if File.directory?(data_dir) 
	Dir.foreach(data_dir) { |file| 
		begin
			f = File.new(data_dir+"/"+file, "r") unless file == "." || file == ".."
			while(line = f.gets)
				puts line
			end
			f.close
		rescue => e
			puts "ERROR! Opening file."
			puts "Exception: #{e}"
			e
		end
	}
else
	puts "ERROR! Cannot find 'data' directory. Please make sure 'data' directory is available in the current directory"
end
	