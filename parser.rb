require 'nokogiri'

class Parser
	def self.parse_xml(xml_document)
		begin
			doc = Nokogiri::XML(xml_document)
		rescue => e
			e
		end
	end

	def self.parse_xml_fragments(xml_document, tag)
		begin
			nodes = Nokogiri::XML::DocumentFragment.parse(xml_document).xpath(tag)
		rescue => e
			e
		end
	end
end