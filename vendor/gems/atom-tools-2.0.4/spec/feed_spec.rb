require File.dirname(__FILE__) + '/spec_helper'

require 'atom/feed'

describe Atom::Entry do
  describe 'extensions' do
    before(:each) do
      @feed = Atom::Feed.parse(fixtures('feed-w-ext'))
    end

    it 'should preserve namespaces' do
      @feed.to_s.should =~ /purl/

      feed2 = Atom::Feed.new
      feed2.merge! @feed

      feed2.to_s.should =~ /purl/
    end
  end
end
