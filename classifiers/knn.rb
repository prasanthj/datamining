class KNN
	def self.get_training_set_knn(dm_format, td_format)
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

	def self.get_testing_set_knn(dm_format, td_format)
		output = []
		raise(ArgumentError, 'Size of dm_format data and tf_format data are not equal') unless dm_format.size == td_format.size
		td_format.each_with_index do |item,idx|
			topics = item['topics']
			if topics == "unknown"
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
end