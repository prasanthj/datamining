require "./stemmer.rb"
require "iconv"

# include stemming module to string class
class String
	include Stemmable
end

class Utils

	# ignoring encoding related errors
	@@ic = Iconv.new('UTF-8//IGNORE', 'UTF-8')

	# normalize function performs the following actions
	# on string or array of string. If the string is 
	# a sentence then this function will tokenize and then
	# apply the following normalizations.
	# - convert multi-line strings to single line string
	# - lowercase
	# - strip to remove leading and trailing whitespaces
	# - remove punctuations (?,!.)
	# - remove numbers (configurable)
	# - stemming (replace with root words) (configurable)
	def self.normalize(inp)
		out = nil
		if inp.class == String

			# reference: http://po-ru.com/diary/fixing-invalid-utf-8-in-ruby-revisited/
			inp = @@ic.iconv(inp + ' ')[0..-2]
			out = inp.gsub(/(?<!\n)\n(?!\n)/,' ').downcase.strip.split(/\W+/)

			out = perform_filtering(out)
		
			# if stemming is enable perform stemming
			if ENABLE_STEMMING == true
				out = perform_stemming(out)
			end
		elsif inp.class == Array
			inp.delete_if { |x| x == nil }
			out = inp.map { |val|
				val = val.gsub(/(?<!\n)\n(?!\n)/,' ').downcase.strip.split(/\W+/) 

				val = perform_filtering(val).join(",")
			}

			if ENABLE_STEMMING == true
				out = perform_stemming(out)
			end
		end

		return out
	end

	private 
	def self.perform_filtering(inp)
		# check if numbers needs to be filtered
		if FILTER_NUMBERS == false
			inp = inp.reject {|s| s.empty?}
		else
			inp = inp.reject {|s| s.empty? || s.to_i != 0 }
		end

		# check if short words needs to be filtered
		if FILTER_WORDS_LESS_THAN > 1
			inp = inp.reject {|s| s if s.length < FILTER_WORDS_LESS_THAN }
		end

		inp
	end

	private
	def self.perform_stemming(inp) 
		if inp.class == String
			inp.stem
		else
			inp.map { |e| e.stem }
		end
	end

	# given the input directory this method loads all the files
	# within the directory. if tag is specified this function
	# uses normal xml parsing else it uses document fragment
	# parsing (files with multiple roots)
	def self.load_files_and_parse(input_dir, root = "")
		doc = {}
		if File.directory?(input_dir) 
			Dir.foreach(input_dir) { |file| 
				begin
					if !(file == "." || file == "..")
						file = input_dir + "/" + file
						if( root == "" )
							fd = File.open(file)
							doc[file] = Parser.parse_xml_fragments(fd)
							fd.close
						else
							doc[file] = Parser.parse_xml_fragments(File.read(file), root)
						end
					end
				rescue => e
					puts "Exception: #{e}"
					e
				end
			}
		else
			puts "ERROR! Cannot find 'data' directory. Please make sure 'data' directory is available in the current directory"
		end

		doc
	end

	def self.elapsed_time_msec(start, finish)
   		(finish - start) * 1000.0
	end
end
