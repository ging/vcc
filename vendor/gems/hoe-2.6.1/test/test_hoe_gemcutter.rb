require "hoe/gemcutter" unless defined? Hoe::Gemcutter

class TestHoeGemcutter < MiniTest::Unit::TestCase
  include Hoe::Gemcutter

  def test_gemcutter_tasks_defined
    define_gemcutter_tasks
    assert Rake::Task[:release_to_gemcutter]
    assert Rake::Task[:release_to].prerequisites.include?('release_to_gemcutter')
  end

  # TODO add tests for push once using Gem::Commands::Push (waiting on rubygems release)
end
