require "rubygems"
require "bundler/setup"
require "yaml"

require './parser.rb'
require './utils.rb'
require './tf-idf.rb'

class Main
	def self.get_xml_map(data_dir)
		# get multi-rooted parsed xml document
		# key is the document name
		# value contains the DOM elements
		parsed_doc = Utils.load_files_and_parse(data_dir, 'REUTERS')
		docid = 0

		# this map will store docid as key
		# value is a map with topics, titles, body fields
		# all other fields will be filtered 
		doc_map = {}
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
		doc_map
	end

	private
	def self.get_topics(value)
		out = value.map do |node| 
			if node['TOPICS'] == 'YES'
				node.xpath('TOPICS/D').map { |val| val.text }.join(",")
			elsif node['TOPICS'] == 'NO'
				"Unknown"
			end
		end
	end

	public 
	def self.run
		parse_config_file("./config.yml")
		pretty_print_config()
		if $save_output == false
			corpus = []
			Main.get_xml_map(File.path("data2")).each do |k,v|
			 	doc_str = v['topics'] + v['contents']
			 	corpus.push(doc_str.split(","))
			end
			tf_idf_corpus = compute_tf_idf(corpus)
			puts tf_idf_corpus.to_s
		else
			begin
				CSV.open($output_file, "wb") do |csv|
					# do the writing here
				end
			rescue => e
				puts "Exception: #{e}"
			end
		end
	end

	private 
	def self.compute_tf_idf(corpus)
		tfidf = TFIDF.new(corpus)
		tf_idf_corpus = tfidf.get_tf_idf()
		if $retain_top_k_words > -1
			new_tf_idf_corpus = []
			tf_idf_corpus.each_with_index do |doc|
				doc = doc.slice(0,$retain_top_k_words-1)
				new_tf_idf_corpus.push(doc)
			end
			tf_idf_corpus = new_tf_idf_corpus
		end
		tf_idf_corpus
	end

	private
	def self.parse_config_file(yaml_file)
		@config = YAML.load_file(yaml_file)
		# set all the keys as global variables
		@config.each { |key, value| eval "$#{key} = value" }
	end

	private
	def self.pretty_print_config
		puts "CONFIGURATIONS (from config.yml file)"
		puts "====================================="
		puts "enable_stemming: " + $enable_stemming.to_s
		puts "filter_numbers: " + $filter_numbers.to_s
		puts "filter_words_less_than: " + $filter_words_less_than.to_s
		puts "retain_top_k_words: " + $retain_top_k_words.to_s
		puts "save_output: " + $save_output.to_s
		puts "output_file: " + $output_file.to_s
		puts "persist_tf_idf_score: " + $persist_tf_idf_score.to_s
		puts "=====================================\n"
	end
end

# execute the application
Main.run