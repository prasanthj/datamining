require "rubygems"
require "yaml"
require "csv"

require './parser.rb'
require './utils.rb'
require './tf-idf.rb'

# This file mainly deals with running the application,
# parsing the input data and writing the output data.
# After parsing the input data, it uses normalization
# defined in utils.rb, followed by ranking using tfidf.
# Finally output is stored in different formats for use
# with other data mining softwares.
class Main
	public
	def self.run(data_dir, config_file)
		# parsing and printing configurations
		parse_config_file(config_file)
		pretty_print_config()

		# converting input docs to hash
		xml_map = get_xml_map(File.path(data_dir))

		# transaction data corpus will include only the contents of body of each doc
		# data matrix corpus will include both topics and contents of each doc
		td_corpus = []
		dm_corpus = []
		xml_map.each do |k,v|
		 	doc_str = v['topics'] + v['contents']
		 	dm_corpus.push(doc_str.split(","))
		 	td_corpus.push(v['contents'].split(","))
		end
		print "Computing tf-idf scores and filtering the contents in document..."
		tf_idf_corpus = compute_tf_idf(td_corpus)
		puts "[SUCCESS]"

		topics_idx_map = get_topics_term_counts(xml_map)
		td_format = get_trasaction_data_format(topics_idx_map, tf_idf_corpus)
		dm_format = get_data_matrix_format(Utils.get_indexed_corpus_with_term_freq(dm_corpus), Utils.get_overall_indexed_corpus(dm_corpus))
		arff_header = get_arff_header(dm_format,td_format)
		arff_data = get_arff_data(dm_format,td_format)
		write_to_output_files(td_format,dm_format,arff_header,arff_data)
	end

	# private methods
	private 
	def self.write_to_output_files(td_format,dm_format,arff_header,arff_data)
		if $save_output == true	
			begin
				# writing data in transaction data format
				outfile_tdf= "output-tdf.csv" 
				print "Storing output in transaction data format to " + outfile_tdf + "..."
				CSV.open(outfile_tdf, "wb") do |csv|
					header = ["docid", "contents", "topics"]
					csv << header
					td_format.each do |doc|
						csv << [doc["docid"], doc["contents"], doc["topics"]]
					end
				end
				puts "[SUCCESS]"

				# writing data in data matrix format
				outfile_dmf="output-dmf.csv"
				print "Storing output in data matrix format to " + outfile_dmf + "..."
				CSV.open(outfile_dmf, "wb") do |csv|
					header = []
					header.push("docid")
					dm_format.first.keys.each do |key|
						header.push(key)
					end
					header.push("topics")
					csv << header
					dm_format.each_with_index do |doc, idx|
						val = []
						val.push(idx)
						doc.values.each do |v|
							val.push(v)
						end
						topics = td_format[idx]['topics']
						val.push(topics)
						csv << val
					end
				end
				puts "[SUCCESS]"

				# writing data in ARFF format (for using it in WEKA)
				outfile_dmf_arff="output-dmf.arff"
				outfile_dmf_arff_tmp="output-dmf.arff.tmp"
				print "Storing output in Attribute-Relation File Format (ARFF) to " + outfile_dmf_arff + "..."
				# there is a little hack here to attach headers to .arff format
				# ruby CSV doesn't support append mode and so data is written to
				# a tmp file first. The header is written to output file and the
				# contents of tmp file is appended to output file. Finally the tmp
				# file is deleted.
				CSV.open(outfile_dmf_arff_tmp, "wb") do |csv|
					arff_data.each do |line|
						csv << line
					end
				end
				File.open(outfile_dmf_arff, 'w') do |f| 
					f.write(arff_header)
					File.open(outfile_dmf_arff_tmp).each_line do |line|
						f.write(line)
					end
				end
				File.delete(outfile_dmf_arff_tmp)
				puts "[SUCCESS]"
			rescue => e
				puts "Exception: #{e}"
			end
		end
	end

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
		parsed_doc.each do |k,v|
			content_map = {}
			content_map['topics'] = Utils.normalize(get_topics(v), true).join(",")
			titles = Utils.normalize(v.xpath('TEXT/TITLE').text)
			body = Utils.normalize(v.xpath('TEXT/BODY').text)
			# combine titles and body arrays
			contents = (titles << body).flatten.compact.join(",")
			content_map['contents'] = contents
			doc_map[docid] = content_map
			docid += 1
		end
		puts "[SUCCESS]"
		doc_map
	end

	def self.get_topics(node)
		out = "unknown"
		if node['TOPICS'] == 'YES'
			out = node.xpath('TOPICS/D').map { |val| val.text }.join(",") 
		end
		out = "unknown" if out.empty?
		out
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
		
		new_tf_idf_corpus = []
		tf_idf_corpus.each_with_index do |doc|
			if $retain_top_k_words > -1
				doc = Utils.get_top_k_from_hash(doc, $retain_top_k_words)
			else
				doc = Utils.get_top_k_from_hash(doc, doc.length)
			end
			new_tf_idf_corpus.push(doc)
		end
		tf_idf_corpus = new_tf_idf_corpus
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

	def self.get_data_matrix_format(doc_corpus, indexed_corpus)
		dmformat = []
		indexed_corpus_clean = Utils.perform_corpus_cleaning(indexed_corpus)
		if $retain_top_k_words > -1
			indexed_corpus_clean = Utils.get_top_k_from_hash(indexed_corpus_clean, $retain_top_k_words)
		end
		
		doc_corpus.each do |doc|
			dmap = {}
			indexed_corpus_clean.each do |k,v|
				dmap[k] = doc[k]
			end
			dmformat.push(dmap)
		end

		dmformat
	end

	def self.get_arff_header(dm_format,td_format)
		header = []
		header << "@RELATION dmf"
		header << ""
		header << "@ATTRIBUTE docid NUMERIC"
		dm_format.first.each do |k,v|
			header << "@ATTRIBUTE " + k + " NUMERIC"
		end

		# populate class labels
		labels = []
		td_format.each do |item|
			topic = item['topics']
			labels = (labels << topic.split(',')).flatten if topic != 'unknown'	
		end
		labels.uniq!
		class_labels = "{"
		class_labels+= labels.join(',')
		class_labels += "}"
		header << "@ATTRIBUTE topic " + class_labels 
		header << ""
		header << "@DATA\n"
		header.join("\n")
	end

	def self.get_arff_data(dm_format,td_format)
		output = []
		raise 'Size of dm_format data and tf_format data are not equal' unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics != "unknown"
				tokens = topics.split(',')
				tokens.each do |topic|
					val = []
					val.push(idx)
					val = (val << dm_format[idx].values).flatten
					val.push(topic)
					output.push(val)
				end
			end
		end

		output
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