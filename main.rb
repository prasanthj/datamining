require "rubygems"
require "yaml"
require "benchmark"

require './parser.rb'
require './utils.rb'
require './tf-idf.rb'
require './io.rb'
require './classifiers/knn.rb'

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
		IO.parse_config_file(config_file)
		IO.pretty_print_config()

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

		IO.print_step("Computing tf-idf scores and filtering the contents in document")
		tf_idf_corpus = compute_tf_idf(td_corpus)
		IO.print_success

		IO.print_step("Preparing data for various output formats")
		topics_idx_map = get_topics_term_counts(xml_map)
		td_format = get_trasaction_data_format(topics_idx_map, tf_idf_corpus)
		dm_format = get_data_matrix_format(Utils.get_indexed_corpus_with_term_freq(dm_corpus), Utils.get_overall_indexed_corpus(dm_corpus))
		arff_header = get_arff_header(dm_format,td_format)
		arff_data = get_arff_data(dm_format,td_format)
		arff_data_binary = get_arff_data_binary(dm_format,td_format)
		if($sample < 1.0) 
			arff_data_sample = Utils.get_random_sample(arff_data, $sample, $seed)
			arff_data_binary_sample = Utils.get_random_sample(arff_data, $sample, $seed)
			arff_data = arff_data_sample
			arff_data_binary = arff_data_binary_sample
		end
		IO.print_success

		write_to_output_files(td_format,dm_format,arff_header,arff_data,arff_data_binary)

		if $enable_classifier == true
			IO.print_step("Preparing training and testing data for classifiers")
			training_set_knn = KNN.get_training_set(dm_format, td_format)
			testing_set_knn = KNN.get_testing_set(dm_format, td_format)
			IO.print_success

			write_classifier_output_files($classifier, training_set_knn, testing_set_knn)

			knn = nil
			train_set_loc = "./output/knn/training/100/training_set.csv"
			# null/empty strings removed from test_data
			test_data = "bank,billion,price,corp,market,loss,offer,stock,trade,rate,issu,debt,week,note,februari,expect,secur,industri,exchang,propos,januari,loan,sell,quarter,foreign,servic,current,analyst,presid,continu,time,financi,brazil,major,approv,spokesman,earlier,file,commiss,hold,term,recent,payment,fall,receiv,fell,lead,reduc,lower,initi,improv,commun,prefer,proce,convert,creditor,believ,condit,equiti,factor,holder,restructur,poor,brazilian,takeov,pressur,immedi,vice,recommend,fail,determin,unknown"
			IO.print_step("Running KNN classifier")
			puts "Training dataset location: " + train_set_loc
			puts "Testing data: " + test_data
			topic_mh = nil
			topic_jc = nil
			Benchmark.bmbm do |x|
				x.report("Offline cost: ") { knn = KNN.new(train_set_loc, true, true) }
				x.report("Online cost(MinHash):") { topic_mh = knn.classify_using_minhash(test_data, false)}
				x.report("Online cost(Jaccard): ") { topic_jc = knn.classify_using_jaccard(test_data, false) }
			end
			puts ""
			puts "Topic predicted by KNN classifier using Jaccard Coefficient: " + topic_jc
			puts "Topic predicted by KNN classifier using MinHash: " + topic_mh
		end


		cluster_in_file = "./output/output-dmf-binary.arff"
		cluster_out_file = "./output/cluster.arff"
		if(File.exists?(cluster_out_file))
			IO.print_step("Computing the quality of clustering output")
			quality = Utils.get_cluster_quality(cluster_in_file, cluster_out_file)
			IO.print_success

			IO.pretty_print_clusters(quality)
			print "Overall quality of this clustering arrangement: "
			overallEnt = 0
			quality.each do |item|
				overallEnt += item["weighted-entropy"]
			end
			puts overallEnt.to_s
		else
			puts "Use " + cluster_in_file + " in WEKA for clustering."
			puts "Save the cluster output to " + cluster_out_file + " and "\
			"rerun this application with -cq option (./run.sh -cq) to find the quality of the clusters generated."
		end
	end

	public 
	def self.run_cluster_quality
		cluster_in_file = "./output/output-dmf-binary.arff"
		cluster_out_file = "./output/cluster.arff"
		if(File.exists?(cluster_out_file))
			IO.print_step("Computing the quality of clustering output")
			quality = Utils.get_cluster_quality(cluster_in_file, cluster_out_file)
			IO.print_success

			IO.pretty_print_clusters(quality)
			print "Overall quality of this clustering arrangement: "
			overallEnt = 0
			quality.each do |item|
				overallEnt += item["weighted-entropy"]
			end
			puts overallEnt.to_s
		else
			puts "Use " + cluster_in_file + " in WEKA for clustering."
			puts "Save the cluster output to " + cluster_out_file + " and "\
			"rerun this application with -cq option (./run.sh -cq) to find the quality of the clusters generated."
		end
	end

	# private methods
	private 
	def self.write_classifier_output_files(classifier_type, training_set, testing_set)
		output_dir = $output_dir + "/" + classifier_type
		IO.print_step("Storing training and testing data sets to " + output_dir + " directory")
		IO.write_classifier_output(output_dir, training_set, testing_set)
		IO.print_success
	end

	def self.write_to_output_files(td_format,dm_format,arff_header,arff_data,arff_data_binary)
		# writing data in transaction data format
		outfile_tdf= "output-tdf.csv" 
		IO.print_step("Storing output in transaction data format to " + outfile_tdf)
		IO.write_transactional_data_as_csv(td_format, $output_dir, outfile_tdf, true);
		IO.print_success

		# writing data in data matrix format
		outfile_dmf="output-dmf.csv"
		IO.print_step("Storing output in data matrix format to " + outfile_dmf)
		IO.write_data_matrix_as_csv(dm_format, td_format, $output_dir, outfile_dmf, true);
		IO.print_success

		# writing data in ARFF format (for using it in WEKA)
		outfile_dmf_arff="output-dmf.arff"
		IO.print_step("Storing data matrix output with term frequencies in Attribute-Relation File Format (ARFF) to " + outfile_dmf_arff)
		IO.write_data_matrix_as_arff(arff_header, arff_data, $output_dir, outfile_dmf_arff, true);
		IO.print_success

		# writing data in ARFF format (for using it in WEKA)
		outfile_dmf_arff="output-dmf-binary.arff"
		IO.print_step("Storing data matrix output with term existence(binary) in Attribute-Relation File Format (ARFF) to " + outfile_dmf_arff)
		IO.write_data_matrix_as_arff(arff_header, arff_data_binary, $output_dir, outfile_dmf_arff, true);
		IO.print_success
	end

	def self.get_xml_map(data_dir)
		# get multi-rooted parsed xml document
		# key is the document name
		# value contains the DOM elements
		IO.print_step("Loading input files from " + data_dir.to_s)
		parsed_doc = Utils.load_files_and_parse(data_dir, 'REUTERS')
		IO.print_success
		docid = 0

		# this map will store docid as key
		# value is a map with topics, titles, body fields
		# all other fields will be filtered 
		doc_map = {}
		IO.print_step("Normalizing the loaded data")
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
		IO.print_success
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
		raise(ArgumentError, 'Size of dm_format data and tf_format data are not equal') unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics != "unknown"
				tokens = topics.split(',')
				tokens.each_with_index do |topic,i|
					if i < 1
						val = []
						val.push(idx)
						val = (val << dm_format[idx].values).flatten
						val.push(topic)
						output.push(val)
					end
				end
			end
		end

		output
	end

	def self.get_arff_data_binary(dm_format,td_format)
		output = []
		raise(ArgumentError, 'Size of dm_format data and tf_format data are not equal') unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics != "unknown"
				tokens = topics.split(',')
				tokens.each_with_index do |topic,i|
					if i < 1
						val = []
						val.push(idx)
						binvals = dm_format[idx].values
						binvals.map! do |item| 
							if item.zero?
								0
							else
								1
							end
						end
						val = (val << binvals).flatten
						val.push(topic)
						output.push(val)
					end
				end
			end
		end

		output
	end
end

# execute the application
if ARGV.size != 0
	Main.run_cluster_quality if ARGV[0] == "-cq"
else
	Main.run("./data", "./config.yml")
end
