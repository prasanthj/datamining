echo "Installing dependencies.."
gem install --user-install nokogiri
gem install --user-install murmurhash3
echo "Executing program.."
ruby main.rb
