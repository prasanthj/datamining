echo "Installing dependencies.."
gem install --user-install nokogiri
gem install --user-install murmurhash3
gem install --user-install multimap
gem install --user-install benzo
echo "Compiling apriori from source.."
(cd lib/apriori/apriori/src;make)
echo "Exporting path to apriori executable.."
export PATH=$PATH:"$PWD/lib/apriori/apriori/src"
echo "Executing program.."
ruby main.rb
