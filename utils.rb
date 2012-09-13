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
			out = inp.gsub(/(?<!\n)\n(?!\n)/,' ').downcase.strip.split(/\W+/).reject{|s| s.empty?}

			out = perform_filtering(out)
		
			# if stemming is enable perform stemming
			if $enable_stemming == true
				out = perform_stemming(out)
			end
		elsif inp.class == Array
			inp = inp.reject { |x| x == nil || x.empty? }
			out = inp.map { |val|
				val = val.gsub(/(?<!\n)\n(?!\n)/,' ').downcase.strip.split(/\W+/) 

				val = perform_filtering(val).join(",")
			}

			if $enable_stemming == true
				out = perform_stemming(out)
			end
		end

		return out
	end

	private 
	def self.perform_filtering(inp)
		# check if numbers needs to be filtered
		if $filter_numbers == true
			inp = inp.reject {|s| s.to_i != 0 }
		end

		# check if short words needs to be filtered
		if $filter_words_less_than > 1
			inp = inp.reject {|s| s if s.length < $filter_words_less_than }
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
			puts "ERROR! Cannot find 'data' directory. Please make sure 'data' directory"\
							" is available in the current directory"
		end

		doc
	end

	def self.elapsed_time_msec(start, finish)
   		(finish - start) * 1000.0
	end

	def self.get_top_k_from_hash(h, n) 
		Hash[h.sort_by { |k,v| -v }[0..n-1]]
	end

	# we need indexed corpus for quickly finding if a word
	# exists in a document or not. it can also be used to 
	# find top-k words in the corpus by sorting it.
	# Since corpus is an array of words, its often time 
	# consuming to iterate through all documents just to 
	# find if it exists or not. The indexed corpus is used
	# while finding idf score and for data matrix representation
	public
	def self.get_indexed_corpus_with_term_freq(corpus)
		idx_corpus = []
		corpus.each do |doc|
			idx_doc = Hash.new(0)
			doc.each do |word|
				idx_doc[word] += 1
			end
			idx_corpus.push(idx_doc)
		end
		idx_corpus
	end

	# this method returns single map that contains all the words in 
	# the corpus with frequecies
	def self.get_overall_indexed_corpus(corpus)
		overall_idx_corpus = Hash.new(0)
		corpus.each do |doc|
			doc.each do |word|
				overall_idx_corpus[word] += 1
			end
		end
		overall_idx_corpus
	end

	def self.perform_corpus_cleaning(tf_idf_corpus)
		stemmed_stop_words = STOPWORDS
		if $enable_stemming == true
			stemmed_stop_words = perform_stemming(STOPWORDS)
		end
		
		stemmed_stop_words.each do |word|
			tf_idf_corpus.delete(word)
		end

		tf_idf_corpus = perform_filtering(tf_idf_corpus)
		
		tf_idf_corpus
	end
end

# stopwords are used for removing top-k words in data matrix representation
STOPWORDS = %w[
a about above across after again against all almost alone along already also
although always among an and another any anybody anyone anything anywhere apos
are area areas around as ask asked asking asks at away

back backed backing backs be became because become becomes been before began
behind being beings best better between big both but by

came can cannot case cases certain certainly clear clearly come could

did differ different differently do does done down down downed downing downs
during

each early either end ended ending ends enough even evenly ever every everybody
everyone everything everywhere

face faces fact facts far felt few find finds first for four from full fully
further furthered furthering furthers

gave general generally get gets give given gives go going good goods got great
greater greatest group grouped grouping groups

had has have having he her here herself high high high higher highest him
himself his how however i if important in interest interested interesting
interests into is it its it's itself

just

keep keeps kind knew know known knows

large largely last later latest least less let lets like likely long longer
longest

made make making man many may me member members men might more most mostly mr
mrs much must my myself

nbsp necessary need needed needing needs never new new newer newest next no
nobody non noone not nothing now nowhere number numbers

of off often old older oldest on once one only open opened opening opens or
order ordered ordering orders other others our out over

part parted parting parts per perhaps place places point pointed pointing points
possible present presented presenting presents problem problems put puts

quite quot

rather really right right room rooms

said same saw say says second seconds see seem seemed seeming seems sees several
shall she should show showed showing shows side sides since small smaller
smallest so some somebody someone something somewhere state states still still
such sure

take taken than that the their them then there therefore these they thing things
think thinks this those though thought thoughts three through thus to today
together too took toward turn turned turning turns two

under until up upon us use used uses

very

want wanted wanting wants was way ways we well wells went were what when where
whether which while who whole whose why will with within without work worked
working works would

year years yet you young younger youngest your yours
]