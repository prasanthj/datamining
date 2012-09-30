echo "Installing dependencies.."
gem install --user-install nokogiri
gem install --user-install murmur_hash
echo "Executing program.."
ruby main.rb
