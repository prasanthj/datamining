require "test/unit"
require File.expand_path(File.dirname(__FILE__) + '/../lib/minhash.rb')

class TestMinHash < Test::Unit::TestCase
	def test_two_sets_similarity
		a = ["hello", "world", "from", "ruby"]
		b = ["ruby", "world", "from", "hello"]
		assert_equal(a.minhash, b.minhash, "Minhash of A: " + a.minhash.to_s + " Minhash of B: " + b.minhash.to_s)
	end

	def test_two_sets_with_slight_dissimilarity
		a = ["hello", "world", "from", "python"]
		b = ["ruby", "world", "from", "hello"]
		assert_equal(a.minhash, b.minhash, "Minhash of A: " + a.minhash.to_s + " Minhash of B: " + b.minhash.to_s)
	end
end