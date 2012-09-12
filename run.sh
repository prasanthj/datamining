#!/bin/bash
echo "Switching to bash shell.."
bash
echo "Installing dependencies.."
gem install --user-install nokogiri
echo "Executing program.."
ruby main.rb
