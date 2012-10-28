require 'csv'
require 'fileutils'

class IO
	# NOTE: In all cases if dir is not provided the output will be written to current directory
	# if write_header is set to true then header information will be written to first line
	# Exceptions are not catched. So its upto the application to handle it.

	# write transactional data format as csv
	def self.write_transactional_data_as_csv(td_format, dir, outfile_tdf, write_header)
		begin
			test_and_create_dir(dir)
			outfile_tdf = "./" + dir.to_s + "/" + outfile_tdf
			CSV.open(outfile_tdf, "wb") do |csv|
				header = ["docid", "contents", "topics"] if write_header
				csv << header if write_header
				td_format.each do |doc|
					csv << [doc["docid"], doc["contents"], doc["topics"]]
				end
			end
		rescue Exception => e
			puts "Exception: #{e}"
			puts e.backtrace
		end
	end

	# write data matrix format as csv
	# we need to pass td_format data for extracting the topics 
	def self.write_data_matrix_as_csv(dm_format, td_format, dir, outfile_dmf, write_header)
		begin
			test_and_create_dir(dir)
			outfile_dmf = "./" + dir.to_s + "/" + outfile_dmf
			CSV.open(outfile_dmf, "wb") do |csv|
					header = []
					header.push("docid")
					dm_format.first.keys.each do |key|
						header.push(key)
					end
					header.push("topics")
					csv << header if write_header
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
		rescue Exception => e
			puts "Exception: #{e}"
			puts e.backtrace
		end
	end

	# write data matrix format as arff
	def self.write_data_matrix_as_arff(arff_header, arff_data, dir, arff_out_file, write_header)
		begin
			test_and_create_dir(dir)
			# there is a little hack here to attach headers to .arff format
			# ruby CSV doesn't support append mode and so data is written to
			# a tmp file first. The header is written to output file and the
			# contents of tmp file is appended to output file. Finally the tmp
			# file is deleted.
			arff_out_file = "./" + dir.to_s + "/" + arff_out_file
			arff_out_file_tmp = arff_out_file + ".tmp"
			CSV.open(arff_out_file_tmp, "wb") do |csv|
				arff_data.each do |line|
					csv << line
				end
			end
			File.open(arff_out_file, 'w') do |f| 
				f.write(arff_header) if write_header
				File.open(arff_out_file_tmp).each_line do |line|
					f.write(line)
				end
			end
			File.delete(arff_out_file_tmp)
		rescue Exception => e
			puts "Exception: #{e}"
			puts e.backtrace
		end
	end

	# writes the classifier output to the specified output directory
	# the default split percentage is 100%. if any other split percentage is
	# specified then the specified percentage is randomly chosen from training set
	# and store inside output_dir. For example: if split_percent=80 then 80% of
	# training set data is randomly chosen and stored inside output_dir/training/80/
	# and remaing 20% is stored inside output_dir/testing/20/. If split_percent is
	# 100% then testing_set is expected (but not mandatory). 
	# If split_percent is a list of split percentages then as many output 
	# directories will be created and training_set will be splitted accordingly
	def self.write_classifier_output(output_dir, training_set, testing_set, split_percent=100)
		begin
			if training_set == nil or training_set.empty?
				raise(ArgumentError, "Training set is nill")
			end

			if split_percent.class != Array and split_percent > 100
				raise(ArgumentError, "Split cannot be greater than 100%")
			end

			train_dir = output_dir + "/training/" 
			test_dir = output_dir + "/testing/" 
			test_and_create_dir(output_dir)
			test_and_create_dir(train_dir)
			test_and_create_dir(test_dir)
			if split_percent.class == Array
				split_percent.each do |percent|
					if percent > 100
						raise(ArgumentError, "Split cannot be greater than 100%")
					end
					out_train_dir = train_dir + percent.to_s
					out_test_dir = test_dir + (100 - percent).to_s
					test_and_create_dir(out_train_dir) 
					test_and_create_dir(out_test_dir) 
					write_classifier_output_impl(out_train_dir, out_test_dir, training_set, testing_set, percent)
				end
			else
				out_train_dir = train_dir + split_percent.to_s
				out_test_dir = test_dir + (100 - split_percent).to_s
				test_and_create_dir(out_train_dir) 
				test_and_create_dir(out_test_dir) 
				write_classifier_output_impl(out_train_dir, out_test_dir, training_set, testing_set, split_percent)
			end
		rescue Exception => e
			puts "Exception: #{e}"
			puts e.backtrace
		end
	end

	private 
	def self.write_classifier_output_impl(train_dir, test_dir, training_set, testing_set, split_percent)
		if split_percent < 100
			# shuffle the list
			training_set.shuffle!
			len = training_set.size
			plen = len * split_percent/100
			train_set = training_set.slice(0..plen-1)
			test_set = training_set.slice(plen..len)
			training_set = train_set
			testing_set = test_set
		end

		CSV.open(train_dir + "/training_set.csv", "wb") do |csv|
			training_set.each do |row|
				csv << row
			end
		end

		if testing_set
			CSV.open(test_dir + "/testing_set.csv", "wb") do |csv|
				testing_set.each do |row|
					csv << row
				end
			end
		end
	end

	public
	def self.parse_config_file(yaml_file)
		@config = YAML.load_file(yaml_file)
		# set all the keys as global variables
		@config.each { |key, value| eval "$#{key} = value" }
	end

	def self.print_step(msg)
		print "- " + msg + "..."
	end

	def self.print_success
		puts "[SUCCESS]"
	end

	def self.pretty_print_clusters(clusters)
		clusters.each do |c|
			c.each do |k,v|
				print k + ":"
				puts v.to_s
			end
			puts ""
		end
	end

	public
	def self.pretty_print_config
		puts "====================================="
		puts "CONFIGURATIONS (from config.yml file)"
		puts "====================================="
		puts "enable_stemming: " + $enable_stemming.to_s
		puts "filter_numbers: " + $filter_numbers.to_s
		puts "filter_words_less_than: " + $filter_words_less_than.to_s
		puts "retain_top_k_words: " + $retain_top_k_words.to_s
		puts "output_dir: " + $output_dir.to_s
		puts "classifier: " + $classifier.to_s
		puts "split: " + $split.to_s
		puts "sample: " + $sample.to_s
		puts "seed: " + $seed.to_s
		puts "=====================================\n"
	end

	private
	def self.test_and_create_dir(dir)
		if dir == nil or dir.empty? == true; return end
		FileUtils.mkpath dir
	end
end