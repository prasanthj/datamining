class TFIDF
	@indexed_corpus = nil
	@idfscores = nil

	# the corpus is expected to contain array of array of words
	# in each doc. array.size should reflect the total number of 
	# docs in the corpus and each element inside the array should 
	# be an array of words inside a single doc
	# Ex: [["word1_1"], ["word_2_1", "word_2_2"], ["word_3_1"]] 
	attr_accessor :corpus
	def initialize(corpus)
		@corpus = corpus
		prepare_idf_scores
	end

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
		Math.log10( (@corpus.length.to_f) / ( 1.0 + @idfscores[term] ) )
	end

	private 
	def prepare_idf_scores
		@indexed_corpus = Utils.get_indexed_corpus_with_term_freq(@corpus) if @indexed_corpus == nil
		@idfscores = Hash.new(0)
		keys = []
		@indexed_corpus.each do |doc|
			doc.each do |k,v|
				@idfscores[k] += 1
			end
		end
	end

	# this method calculates the tf_idf scores for a given document corpus.
	# tf_idf = tf * idf
	# refer get_tf and get_idf functions for more description.
	# the output is also sorted so that words with lower scores 
	# are arranged at the bottom of the list
	public
	def get_tf_idf
		tf_idf_overall = []
		@indexed_corpus = Utils.get_indexed_corpus_with_term_freq(@corpus) if @indexed_corpus == nil
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

end