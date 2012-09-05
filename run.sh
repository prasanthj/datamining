#!/bin/bash
set -e
gem install bundle
bundle install
ruby main.rb
