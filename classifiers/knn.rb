require './lib/minhash.rb'

class KNN
	@training_set_minhash = nil
	# training_set can be array or file
	# if it is file then isFile attribute should be set
	# if useMinHash is set to true then hashmap of minhash and 
	# its corresponding topic will be stored internally (offline cost)
	attr_accessor :training_set
	def initialize(training_set, isFile=false, useMinHash=false)
		if isFile
			@training_set = []
			File.open(training_set).each_line do |line|
				@training_set.push(line)
			end
		else
			@training_set = training_set
		end

		if useMinHash
			@training_set_minhash = {}
			@training_set.each do |line|
				line.chomp!
				larr = line.split(",")
				topic = larr.last
				mh = line.split(",").slice(0..larr.size-2)
				mh = mh.reject { |x| x.empty? }
				@training_set_minhash[mh.minhash] = topic
			end
		end
		
	end

	def self.get_training_set_for_jaccard(dm_format, td_format)
		output = []
		raise(ArgumentError, 'Size of dm_format data and tf_format data are not equal') unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics != "unknown"
				tokens = topics.split(',')
				tokens.each_with_index do |topic,i|
					if i < 1
						val = []
						val = (val << dm_format[idx].values).flatten
						val.push(topic)
						output.push(val)
					end
				end
			end
		end

		output
	end

	def self.get_training_set_for_minhash(dm_format, td_format)
		output = []
		raise(ArgumentError, 'Size of dm_format data and tf_format data are not equal') unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics != "unknown"
				tokens = topics.split(',')
				tokens.each_with_index do |topic,i|
					if i < 1
						val = []
						dm_format[idx].each do |k,v|
							if v == 0
								val.push(nil)
							else
								val.push(k)
							end
						end
						val.push(topic)
						output.push(val)
					end
				end
			end
		end

		output
	end

	def self.get_testing_set_for_jaccard(dm_format, td_format)
		output = []
		raise(ArgumentError, 'Size of dm_format data and tf_format data are not equal') unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics == "unknown"
				tokens = topics.split(',')
				tokens.each_with_index do |topic,i|
					if i < 1
						val = []
						val = (val << dm_format[idx].values).flatten
						output.push(val)
					end
				end
			end
		end

		output
	end

	def self.get_testing_set_for_minhash(dm_format, td_format)
		output = []
		raise(ArgumentError, 'Size of dm_format data and tf_format data are not equal') unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics == "unknown"
				tokens = topics.split(',')
				tokens.each_with_index do |topic,i|
					if i < 1
						val = []
						dm_format[idx].each do |k,v|
							if v == 0
								val.push(nil)
							else
								val.push(k)
							end
						end
						output.push(val)
					end
				end
			end
		end

		output
	end

	# test_set can be array or file
	# if it is file then isFile attribute should be set
	def classify_using_jaccard(testing_set, isFile=false)
		output = []
		if isFile == true
			File.open(testing_set).each_line do |test_line|
				@training_set.each do |train_line|
					jc = jaccard_coefficient(train_line.split(","), test_line.split(",")).to_s
					output.push(jc)
				end
			end
		else

		end

		output
	end

	def classify_using_minhash(testing_set, isFile=false)
		output = []
		if isFile == true
			File.open(testing_set).each_line do |test_line|
				tout = []
				tl = test_line.split(",")
				tl_no_topic = tl.slice(0..tl.size-2).reject { |x| x.empty? }
				tlmh = tl_no_topic.minhash
				predicted_topic = @training_set_minhash[tlmh] 
				tout = (tout << tl_no_topic).flatten
				tout.push(predicted_topic)
				#puts tl_no_topic.to_s + " => " + predicted_topic.to_s
				output.push(tout)
			end
		else
			tl = testing_set.split(",")
			tl_no_topic = tl.slice(0..tl.size-2).reject { |x| x.empty? }
			tlmh = tl_no_topic.minhash
			predicted_topic = @training_set_minhash[tlmh] 
			output = (output << tl_no_topic).flatten
			output.push(predicted_topic)
			output = output.join(",")
		end

		output
	end

	private
	# sim(S1,S2) = |S1 n S2| / |S1 u s2|
	# while calculating jaccard coefficient we will
	# ignores the pairs that are both '0' i.e, if a particular
	# word is not present in both sets then we can ignore it 
	# while calculating union. if size of both sets are not 
	# equal we will crop the remaining elements from the larger set
	def jaccard_coefficient(set1,set2)
		union_score = 0
		intersection_score = 0
		if set1.size < set2.size 
			set2 = set2.slice(0..set1.size-1)
		elsif set1.size > set2.size
			set1 = set1.slice(0..set2.size-1)
		end

		set1.each_with_index do |elem,idx|
			next if elem == 0 and set2[idx] == 0
			if elem != 0 and set2[idx] != 0
				union_score += 1
				intersection_score +=1
			else
				union_score += 1
			end
		end

		intersection_score/union_score if union_score != 0
	end

	private 
	def locality_sensitive_hash
		# TODO
	end
end