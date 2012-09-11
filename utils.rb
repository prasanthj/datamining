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
	# 1: lowercase
	# 2: strip to remove leading and trailing whitespaces
	# 3: split (incase of sentence)
	# 4: chomp to remove \n and \r
	# 5: remove punctuations (?,!.)
	# 6: remove numbers (optional)
	# 7: stemming (replace with root words) (optional)
	def self.normalize(inp)
		out = nil
		if inp.class == String

			# reference: http://po-ru.com/diary/fixing-invalid-utf-8-in-ruby-revisited/
			inp = @@ic.iconv(inp + ' ')[0..-2]
			puts inp 
			if KEEP_NUMBERS == true
				out = inp.downcase.strip.split(/\W+/).reject {|s| s.empty?}
			else
				out = inp.dump.downcase.strip.split(/\W+/).reject {|s| s.empty? || s.to_i != 0 }
			end

			if ENABLE_STEMMING == true
				out = perform_stemming(out)
			end
		elsif inp.class == Array
			inp.delete_if { |x| x == nil }
			out = inp.map { |val|
				if KEEP_NUMBERS == true
					val.downcase.strip.split(/\W+/).reject {|s| s.empty?}.join(",")
				else
					val.downcase.strip.split(/\W+/).reject {|s| s.empty? || s.to_i != 0}.join(",")
				end
			}

			if ENABLE_STEMMING == true
				out = perform_stemming(out)
			end
		end

		return out
	end

	private
	def self.perform_stemming(inp) 
		if inp.class == String
			inp.stem
		else
			inp.map { |e| e.stem }
		end
	end

	def self.get_tf_idf_score(corpus)
		tf_idf_overall = []
		corpus_freqs = get_word_freq_in_corpus(corpus)
		corpus.each { |doc|
			tf_idf = {}
			get_word_freq_in_doc(doc).each { |term, f|
				rank = f * (1.0/corpus_freqs[term].to_f)
				tf_idf[term] = rank if rank > 0.3
			}
			tf_idf_overall.push(tf_idf)
		}
		puts tf_idf_overall.to_s
	end

	private
	def self.get_word_freq_in_doc(doc)
		freqs = Hash.new(0)
		doc.each { |word|
			freqs[word] += 1
		}
		freqs
	end

	private
	def self.get_word_freq_in_corpus(corpus)
		freqs = Hash.new(0)
		corpus.flatten.compact.each { |word|
			freqs[word] += 1	
		}
		freqs
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

end
