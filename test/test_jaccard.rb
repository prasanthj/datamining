require "test/unit"
require File.expand_path(File.dirname(__FILE__) + '/../classifiers/knn.rb')

class TestJaccard < Test::Unit::TestCase
	def test_similar_sets
		a = [1,2,3,4]
		b = [1,2,3,4]
		jc = KNN.jaccard_coefficient(a,b)
		assert_equal(jc, 1.0, "Expected jc: 1.0 Got: " + jc.to_s)
	end

	def test_half_similar_sets
		a = [1,2,3,4]
		b = [1,2]
		jc = KNN.jaccard_coefficient(a,b)
		assert_equal(jc, 0.5, ("Expected: 0.5 Got: " + jc.to_s))
	end

	def test_string_sets
		a = ["a", "b", "", "", "e"]
		b = ["", "b", "c", "d", ""]
		jc = KNN.jaccard_coefficient(a,b)
		assert_equal(jc, 0.3333333333333333, "Expected: 0.3333333333333333 Got: " + jc.to_s)
	end
end