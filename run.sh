#!/bin/bash
set -e
setenv GEM_HOME "~/.gem"
gem install --user-install nokogiri
ruby main.rb
