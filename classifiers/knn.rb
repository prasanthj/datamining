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

		@training_set_minhash = {}
		if useMinHash
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

	def self.get_training_set(dm_format, td_format)
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

	def self.get_testing_set(dm_format, td_format)
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
						val.push(topic)
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
		prev_jc = 0.0
		output = "unknown"
		if isFile == true
			predicted_topics = []
			File.open(testing_set).each_line do |test_line|
				teset = test_line.split(",")
				teset_no_topic = teset.slice(0..teset.size-2).reject { |x| x.empty? }
				@training_set.each do |train_line|
					trset = train_line.split(",")
					current_topic = trset.last
					trset_no_topic = trset.slice(0..trset.size-2).reject { |x| x.empty? }
					jc = jaccard_coefficient(trset_no_topic, teset_no_topic)
					if prev_jc == 0.0
						prev_jc = jc
					end
					if jc > prev_jc
						prev_jc = jc
						predicted_topic = current_topic
					end
				end
				predicted_topics.push(predicted_topic)
			end
			output = predicted_topics
		else
			teset = testing_set.split(",")
			teset_no_topic = teset.slice(0..teset.size-2).reject { |x| x.empty? }
			predicted_topic = output
			@training_set.each do |train_line|
				trset = train_line.split(",")
				current_topic = trset.last
				trset_no_topic = trset.slice(0..trset.size-2).reject { |x| x.empty? }
				jc = jaccard_coefficient(trset_no_topic, teset_no_topic)
				if prev_jc == 0.0
					prev_jc = jc
				end
				if jc > prev_jc
					prev_jc = jc
					predicted_topic = current_topic
				end
			end
			output = predicted_topic
		end

		output
	end

	def classify_using_minhash(testing_set, isFile=false)
		output = "unknown"
		if isFile == true
			predicted_topics = []
			File.open(testing_set).each_line do |test_line|
				tout = []
				tl = test_line.split(",")
				tl_no_topic = tl.slice(0..tl.size-2).reject { |x| x.empty? }
				tlmh = tl_no_topic.minhash
				predicted_topic = @training_set_minhash[tlmh] 
				predicted_topics.push(predicted_topic)
			end
			output = predicted_topics 
		else
			tl = testing_set.split(",")
			tl_no_topic = tl.slice(0..tl.size-2).reject { |x| x.empty? }
			tlmh = tl_no_topic.minhash
			predicted_topic = @training_set_minhash[tlmh] 
			output = predicted_topic
		end
		output = "unknown" if output == nil
		output
	end

	public
	# sim(S1,S2) = |S1 n S2| / |S1 u s2|
	def jaccard_coefficient(set1,set2)
		union = set1 + set2
		intersection = set1 & set2
		# union set should be unique
		union.uniq!
		intersection.length.to_f/union.length.to_f
	end
end