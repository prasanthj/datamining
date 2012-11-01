require "./stemmer.rb"
require "iconv"
require "benchmark"
require "multimap"

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
	# - replace '-' with '' [Example: "veg-oil" => "vegoil"]
	# - remove punctuations (?,!.)
	# - remove numbers (configurable)
	# - stemming (replace with root words) (configurable)
	def self.normalize(inp, isTopic=false)
		out = nil
		if inp.class == String

			# reference: http://po-ru.com/diary/fixing-invalid-utf-8-in-ruby-revisited/
			inp = @@ic.iconv(inp + ' ')[0..-2]
			out = inp.gsub(/(?<!\n)\n(?!\n)/,' ').downcase.strip.gsub(/\-/,'').split(/\W+/).reject{|s| s.empty?}

			if isTopic == false
				out = perform_filtering(out)
		
				# if stemming is enable perform stemming
				if $enable_stemming == true
					out = perform_stemming(out)
				end
			end
		elsif inp.class == Array
			inp = inp.reject { |x| x == nil || x.empty? }
			out = inp.map { |val|
				val = val.gsub(/(?<!\n)\n(?!\n)/,' ').downcase.strip.gsub(/\-/,'').split(/\W+/) 
				if isTopic == false
					val = perform_filtering(val).join(",")
				end
			}

			if $enable_stemming == true and isTopic == false
				out = perform_stemming(out)
			end
		end

		out = perform_stop_word_filtering(out)
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

	def self.perform_stop_word_filtering(out)
		sw = Hash.new(0)
		
		if $enable_stemming == true
			prepare_stemmed_stopwords
			out.each_with_index do |item,idx|
				if @@stemmed_stopwords.include?(item) == true
					out[idx] = nil 
				end
			end
		else
			out.each_with_index do |item,idx|
				if @@stopwords.include?(item) == true
					out[idx] = nil 
				end
			end
		end
		out = out.compact
		out
	end

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
		nodes = []
		fileid = 0
		if File.directory?(input_dir) 
			Dir.foreach(input_dir) { |file| 
				begin
					if !(file == "." || file == "..")
						file = input_dir + "/" + file
						if( root == "" )
							fd = File.open(file)
							nodes = Parser.parse_xml_fragments(fd)
							nodes.each do |node|
								doc[fileid] = node
								fileid += 1
							end
							fd.close
						else
							nodes = Parser.parse_xml_fragments(File.read(file), root)
							nodes.each do |node|
								doc[fileid] = node
								fileid += 1
							end
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
		if $enable_stemming == true
			prepare_stemmed_stopwords
			@@stemmed_stopwords.each do |word|
				tf_idf_corpus.delete(word)
			end
		else 
			@@stopwords.each do |word|
				tf_idf_corpus.delete(word)
			end
		end
		
		tf_idf_corpus = perform_filtering(tf_idf_corpus)
		
		tf_idf_corpus
	end

	def self.prepare_stemmed_stopwords
		if @@stemmed_stopwords == nil and $enable_stemming == true
			@@stemmed_stopwords = perform_stemming(@@stopwords)
		end
	end

	# given the input weka file in arff format and cluster output
	# file in arff format, this function will compute the 
	# quality of the cluster using weighted entropy
	def self.get_cluster_quality(arff_in_file, arff_out_file)
		clusters = get_last_attribute(arff_out_file)
		topics = get_last_attribute(arff_in_file)
		output = []
		# if number of rows doesn't match in in and out file then return error
		if (clusters.size != topics.size)
			raise(ArgumentError, "Input and ouput ARFF file should have same number of feature vectors. 
				Input vectors: " + topics.size.to_s + " Output vectors: " + clusters.size.to_s) 
		end

		clusternames = clusters.uniq
		clustermap = Multimap.new
		# put all topics belonging to a cluster in a multimap with key as cluster name
		clusters.each_with_index do |v,idx|
			clustermap[v] = topics[idx]
		end

		clusternames.each do |name|
			result = {}
			result["name"] = name
			result["size"] = clustermap[name].size
			ent = get_cluster_entropy(clustermap[name])
			went = (clustermap[name].size.to_f/clusters.size.to_f) * ent
			result["entropy"] = ent
			result["weighted-entropy"] = went
			output.push(result)
		end

		output
	end

	# this is used for saving a random sample of output data. sample size
	# and seed value are configurable through config.yml
	public
	def self.get_random_sample(data, sample, seed = 100)
		result = []
		dataSize = data.length
		totalSize = sample * dataSize
		idx = 0
		r = Random.new(seed)
		while idx < totalSize
			rint = r.rand(0..dataSize)
			result.push(data[rint])
			idx = idx + 1
		end

		result
	end

	# this function computes the entropy of a given cluster
	private 
	def self.get_cluster_entropy(cluster)
		totalsize = cluster.size
		itemCountMap = Hash.new(0)
		overallEnt = 0

		# count the number of unique topics within the cluster
		cluster.each do |item|
			itemCountMap[item] += 1
		end

		# compute entropy of each topic and compute summation of 
		# overall entropy
		itemCountMap.each do |k,v|
			entropy = -(v.to_f/totalsize.to_f) * (Math.log(v.to_f/totalsize.to_f, 2))
			overallEnt = overallEnt.to_f + entropy.to_f
		end

		overallEnt
	end

	private 
	def self.get_last_attribute(arff_file)
		raise(ArgumentError, "Input arff file doesn't exist") if File.exists?(arff_file) == false
		result = []
		File.open(arff_file, "r").each_line do |line|
			val = nil
			begin
				val = Integer(line.chr)
			rescue Exception => e
				next
			end

			if(val != nil)
				result.push(line.split(",").last.chomp)
			end
		end

		result
	end

# stopwords are used for removing top-k words in data matrix representation
@@stemmed_stopwords = nil
@@stopwords = %w[
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

rather really right right room rooms reuter

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

end