echo "Installing dependencies.."
gem install --user-install nokogiri
gem install --user-install murmurhash3
gem install --user-install multimap
echo "Executing program.."
ruby main.rb $1 $2 $3
