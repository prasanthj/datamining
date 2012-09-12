require "rubygems"
require "bundler/setup"
require "yaml"
require "csv"

require './parser.rb'
require './utils.rb'
require './tf-idf.rb'

class Main
	public
	def self.run(data_dir, config_file)
		# parsing and printing configurations
		parse_config_file(config_file)
		pretty_print_config()

		# converting input docs to hash
		xml_map = get_xml_map(File.path(data_dir))
		corpus = []
		xml_map.each do |k,v|
		 	doc_str = v['topics'] + v['contents']
		 	corpus.push(doc_str.split(","))
		end
		print "Computing tf-idf scores and filtering the contents in document..."
		tf_idf_corpus = compute_tf_idf(corpus)
		puts "[SUCCESS]"

		topics_idx_map = get_topics_term_counts(xml_map)
		td_format = get_trasaction_data_format(topics_idx_map, tf_idf_corpus)
		if $save_output == true	
			begin
				outfile = "output-tdf.csv" 
				print "Storing output in transaction data format to " + outfile + "..."
				CSV.open(outfile, "wb") do |csv|
					td_format.each do |doc|
						csv << [doc["docid"], doc["topics"], doc["contents"]]
					end
				end
				puts "[SUCCESS]"
			rescue => e
				puts "Exception: #{e}"
			end
		end
	end

	# private methods
	private 
	def self.get_xml_map(data_dir)
		# get multi-rooted parsed xml document
		# key is the document name
		# value contains the DOM elements
		print "Loading input files from " + data_dir.to_s + "..."
		parsed_doc = Utils.load_files_and_parse(data_dir, 'REUTERS')
		puts "[SUCCESS]"
		docid = 0

		# this map will store docid as key
		# value is a map with topics, titles, body fields
		# all other fields will be filtered 
		doc_map = {}
		print "Normalizing the loaded data..."
		parsed_doc.each { |k,v|
			content_map = {}
			content_map['topics'] = Utils.normalize(get_topics(v)).join(",")
			titles = v.map { |node| Utils.normalize(node.xpath('TEXT/TITLE').text) }
			body = v.map { |node| Utils.normalize(node.xpath('TEXT/BODY').text) }
			# combine titles and body arrays
			contents = titles.zip(body).flatten.compact.join(",")
			content_map['contents'] = contents
			doc_map[docid] = content_map
			docid += 1
		}
		puts "[SUCCESS]"
		doc_map
	end

	def self.get_topics(value)
		out = value.map do |node| 
			if node['TOPICS'] == 'YES'
				node.xpath('TOPICS/D').map { |val| val.text }.join(",")
			elsif node['TOPICS'] == 'NO'
				"Unknown"
			end
		end
	end

	def self.get_topics_term_counts(corpus_map)
		output = []
		corpus_map.each do |k,v|
			term_freq = Hash.new(0)
			v["topics"].split(",").reject{|x| x.empty?}.each do |term|
				term_freq[term] += 1
			end
			output.push(term_freq)
		end
		output
	end

	def self.compute_tf_idf(corpus)
		tfidf = TFIDF.new(corpus)
		tf_idf_corpus = tfidf.get_tf_idf()
		if $retain_top_k_words > -1
			new_tf_idf_corpus = []
			tf_idf_corpus.each_with_index do |doc|
				doc = Utils.get_top_k_from_hash(doc, $retain_top_k_words)
				new_tf_idf_corpus.push(doc)
			end
			tf_idf_corpus = new_tf_idf_corpus
		end
		tf_idf_corpus
	end

	def self.get_trasaction_data_format(topics_freqs_map, tf_idf_corpus)
		# topics_freqs_map is list of maps containing each terms in topics 
		# and their frequency in corresponding document
		# tf_idf_corpus is list of maps containing top-k terms in contents
		# and their tf-idf score
		output = []
		if topics_freqs_map.size == tf_idf_corpus.size
			tf_idf_corpus.each_with_index do |doc, idx|
				output_corpus = {}
				output_corpus["docid"] = idx
				output_corpus["topics"] = topics_freqs_map.at(idx).keys.join(",")
				output_corpus["contents"] = tf_idf_corpus.at(idx).keys.join(",")
				output.push(output_corpus)
			end
			output
		else
			raise "Error! topics_freqs_map length("+topics_freqs_map.size.to_s+") and "\
				"tf_idf_corpus length("+tf_idf_corpus.size.to_s+") are different."
		end
	end

	def self.parse_config_file(yaml_file)
		@config = YAML.load_file(yaml_file)
		# set all the keys as global variables
		@config.each { |key, value| eval "$#{key} = value" }
	end

	def self.pretty_print_config
		puts "====================================="
		puts "CONFIGURATIONS (from config.yml file)"
		puts "====================================="
		puts "enable_stemming: " + $enable_stemming.to_s
		puts "filter_numbers: " + $filter_numbers.to_s
		puts "filter_words_less_than: " + $filter_words_less_than.to_s
		puts "retain_top_k_words: " + $retain_top_k_words.to_s
		puts "save_output: " + $save_output.to_s
		puts "=====================================\n"
	end
end

# execute the application
Main.run("./data", "./config.yml")