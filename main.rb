require "rubygems"
require "bundler/setup"

require './parser.rb'
require './utils.rb'
require './config.rb'
require './tf-idf.rb'

class Main
	def self.get_xml_map(data_dir)
		# get multi-rooted parsed xml document
		# key is the document name
		# value contains the DOM elements
		parsed_doc = Utils.load_files_and_parse(data_dir, 'REUTERS')
		docid = 0

		# this map will store docid as key
		# value is a map with topics, titles, body fields
		# all other fields will be filtered 
		doc_map = {}
		parsed_doc.each { |k,v|
			content_map = {}
			content_map['topics'] = Utils.normalize(get_topics(v)).join(",")
			titles = v.map { |node| Utils.normalize(node.xpath('TEXT/TITLE').text) }
			body = v.map { |node| Utils.normalize(node.xpath('TEXT/BODY').text) }
			# combine titles and body arrays
			contents = titles.zip(body).flatten.compact.join(",")
			content_map['contents'] = contents
			doc_map[docid] = content_map
			docid += 1
		}
		doc_map
	end

	private
	def self.get_topics(value)
		out = value.map do |node| 
			if node['TOPICS'] == 'YES'
				node.xpath('TOPICS/D').map { |val| val.text }.join(",")
			elsif node['TOPICS'] == 'NO'
				"Unknown"
			end
		end
	end

end

if SAVE_OUTPUT == false
	corpus = []
	Main.get_xml_map(File.path("data2")).each do |k,v|
	 	doc_str = v['topics'] + v['contents']
	 	corpus.push(doc_str.split(","))
	end
	tfidf = TFIDF.new(corpus)
	tf_idf_corpus = tfidf.get_tf_idf
	if RETAIN_TOP_K_WORDS > -1
		new_tf_idf_corpus = []
		tf_idf_corpus.each_with_index do |doc|
			doc = doc.slice(0,RETAIN_TOP_K_WORDS-1)
			new_tf_idf_corpus.push(doc)
		end
		tf_idf_corpus = new_tf_idf_corpus
	end
	puts tf_idf_corpus.to_s
else
	begin
		CSV.open(OUTPUT_FILE, "wb") do |csv|
			# do the writing here
		end
	rescue => e
		puts "Exception: #{e}"
	end
end