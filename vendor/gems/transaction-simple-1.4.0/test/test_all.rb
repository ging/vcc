#!/usr/bin/env ruby
#--
# Transaction::Simple
# Simple object transaction support for Ruby
# http://rubyforge.org/projects/trans-simple/
#   Version 1.4.0
#
# Licensed under a MIT-style licence. See Licence.txt in the main
# distribution for full licensing information.
#
# Copyright (c) 2003 - 2007 Austin Ziegler
#
# $Id: test_all.rb 55 2007-02-03 23:29:34Z austin $
#++

$LOAD_PATH.unshift("#{File.dirname(__FILE__)}/../lib") if __FILE__ == $0

$stderr.puts "Checking for test cases:"

Dir[File.join(File.dirname($0), 'test_*.rb')].each do |testcase|
  next if File.basename(testcase) == File.basename(__FILE__)
  $stderr.puts "\t#{testcase}"
  load testcase
end
$stderr.puts " "
