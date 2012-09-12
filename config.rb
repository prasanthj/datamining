# enable stemming of words in the content
ENABLE_STEMMING=true

# if save_output is enabled output_file should 
# be specified
SAVE_OUTPUT=false
OUTPUT_FILE="./output.txt"

# filtering the feature vectors

# keep/drop numbers from the contents
FILTER_NUMBERS=true

# filtering feature vector of each doc
# a value of <=1 will include all words
FILTER_WORDS_LESS_THAN=4

# just retain top-k words in feature vector 
# based on tf-idf ranking
# any value less than 0 will retain all words
RETAIN_TOP_K_WORDS=10