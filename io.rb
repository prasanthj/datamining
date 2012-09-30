require 'csv'

class IO
	# NOTE: In all cases if dir is not provided the output will be written to current directory
	# if write_header is set to true then header information will be written to first line
	# Exceptions are not catched. So its upto the application to handle it.

	# write transactional data format as csv
	def self.write_transactional_data_as_csv(td_format, dir, outfile_tdf, write_header)
		test_and_create_dir(dir)
		outfile_tdf = "./" + dir.to_s + "/" + outfile_tdf
		CSV.open(outfile_tdf, "wb") do |csv|
			header = ["docid", "contents", "topics"]
			csv << header if write_header
			td_format.each do |doc|
				csv << [doc["docid"], doc["contents"], doc["topics"]]
			end
		end
	end

	# write data matrix format as csv
	# we need to pass td_format data for extracting the topics 
	def self.write_data_matrix_as_csv(dm_format, td_format, dir, outfile_dmf, write_header)
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
	end

	# write data matrix format as arff
	def self.write_data_matrix_as_arff(arff_header, arff_data, dir, arff_out_file, write_header)
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

	private
	def self.test_and_create_dir(dir)
		if dir == nil or dir.empty? == true; return end
		Dir.mkdir(dir) if Dir.exists?(dir) == false
	end
end