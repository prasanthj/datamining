require "murmurhash3"

module MinHash
	def minhash
		output = 0
		self.each_with_index do |item, idx|
			if item.class == Fixnum
      			mh = MurmurHash3::V32.int64_hash(item.to_s)
      		elsif item.class == String
      			mh = MurmurHash3::V32.str_hash(item.to_s)
      		end
      		output = mh if idx == 0
      		output = mh if mh < output
    	end

    	output
	end
end


# include minhash module to array class
class Array
	include MinHash
end
