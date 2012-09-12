class TFIDF
	# the corpus is expected to contain array of array of words
	# in each doc. array.size should reflect the total number of 
	# docs in the corpus and each element inside the array should 
	# be an array of words inside a single doc
	# Ex: [["word1_1"], ["word_2_1", "word_2_2"], ["word_3_1"]] 
	attr_accessor :corpus
	def initialize(corpus)
		@corpus = corpus
	end

	@indexed_corpus = nil
	# this function calculates tf scores for a given document.
	# it expects an input array with all words in a doc.
	# tf value is normalized so that there will be no bias when
	# dealing with documents of different sizes
	# tf = term_frequencey/#words_in_doc
	private
	def get_tf doc
		freqs = Hash.new(0)
		doc.each { |word| freqs[word] += 1 }
		freqs.each do |k,v|
			freqs[k] = v.to_f/doc.length.to_f
		end
		
		freqs.sort_by{|x,y| y}
	end

	# this function calculates idf scores for a given document corpus
	# this function makes sure that frequently occuring words are 
	# assigned a lower value i.e, importance of a word
	# idf = log10(total_docs_in_corpus/number_of_docs_containing_the_word)
	private
	def get_idf term
		Math.log10( (@corpus.length.to_f) / ( 1.0 + get_term_freq_in_corpus(term) ) )
	end

	private
	def get_term_freq_in_corpus term
		count = 0;
		@indexed_corpus.each do |doc|
			if doc.has_key?(term) == true
				count += 1
			end
		end
		count.to_f
	end

	# this method calculates the tf_idf scores for a given document corpus.
	# tf_idf = tf * idf
	# refer get_tf and get_idf functions for more description.
	# the output is also sorted so that words with lower scores 
	# are arranged at the bottom of the list
	public
	def get_tf_idf
		tf_idf_overall = []
		prepare_indexed_corpus if @indexed_corpus == nil
		# iterate through docs
		@corpus.each_with_index do |doc, idx|
			tf_scores = get_tf(doc)
			tf_idf_doc = {}
			tf_scores.each do |term, tf|
				rank = tf * get_idf(term)
				# debugging prints
				# if(term == "unknown") 
				# 	puts "DocId: " + idx.to_s
				# 	puts "Term: " + term
				# 	puts "Corpus length: " + @corpus.length.to_s
				# 	puts "Term frequency in corpus: " + get_term_freq_in_corpus(term).to_s
				# 	puts "TF: " + tf.to_s
				# 	puts "IDF: " + get_idf(term).to_s 
				# 	puts "Rank: " + rank.to_s
				# end
				tf_idf_doc[term] = rank
			end
			tf_idf_overall.push(Hash[*tf_idf_doc.sort_by {|k,v| v}.reverse!.flatten])
		end

		tf_idf_overall
	end

	# we need indexed corpus for quickly finding if a word
	# exisits in a document or not. Since @corpus is an array
	# of words, its often time consuming to iterate through
	# all documents just to find if it exists or not. This
	# information is required while calculating idf score
	private
	def prepare_indexed_corpus
		@indexed_corpus = []
		@corpus.each do |doc|
			idx_doc = {}
			doc.each do |word|
				idx_doc[word] = 0
			end
			@indexed_corpus.push(idx_doc)
		end
	end

end