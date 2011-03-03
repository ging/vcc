# Be sure to restart your server when you modify this file

# Specifies gem version of Rails to use when vendor/rails is not present
RAILS_GEM_VERSION = '2.3.4' unless defined? RAILS_GEM_VERSION

# Bootstrap the Rails environment, frameworks, and default configuration
require File.join(File.dirname(__FILE__), 'boot')

Rails::Initializer.run do |config|
  
#   if Gem::VERSION >= "1.3.6" 
#      module Rails
#          class GemDependency
#              def requirement
#                  r = super
#                  (r == Gem::Requirement.default) ? nil : r
#              end
#          end
#      end
#    end
    
  # Settings in config/environments/* take precedence over those specified here.
  # Application configuration should go into files in config/initializers
  # -- all .rb files in that directory are automatically loaded.

  # Allowed html tags for sanitize  
  config.action_view.sanitized_allowed_tags = 'table', 'tr', 'td', 'embed', 'u'
  config.action_view.sanitized_allowed_attributes = 'id', 'class', 'style', 'allowfullscreen', 'wmode'

  # Add additional load paths for your own custom dirs
  # config.load_paths += %W( #{RAILS_ROOT}/extras )

  # Specify gems that this application depends on and have them installed with rake gems:install
  # config.gem "bj"
  # config.gem "hpricot", :version => '0.6', :source => "http://code.whytheluckystiff.net"
  # config.gem "sqlite3-ruby", :lib => "sqlite3"
  # config.gem "aws-s3", :lib => "aws/s3"

  config.gem "rmagick", :version => '2.13.1', :lib => false
  config.gem "rake", :version => '0.8.7', :lib => false
  config.gem "vpim", :version => '0.695', :lib => false
  config.gem "ruby-debug", :version => '0.10.3', :lib => false
  config.gem "ruby-openid", :version => '2.1.7', :lib => false
  config.gem "atom-tools", :version => '2.0.4', :lib => false
  config.gem "mislav-will_paginate", :version => '2.3.11', :lib => false
  config.gem "rcov", :version => '0.9.8', :lib => false
  config.gem "chronic", :version => '0.2.3', :lib => false
  config.gem "hpricot", :version => '0.8.2', :lib => false
  config.gem "feed-normalizer", :version => '1.5.2', :lib => false
  config.gem "rspec-rails", :version => '1.3.2', :lib => false
  config.gem "hoe", :version => '2.6.1', :lib => false
  config.gem "httparty", :version => '0.5.2', :lib => false
  config.gem "pdf-writer", :version => '1.1.8', :lib => false
  config.gem "ci_reporter", :version => '1.6.2', :lib => false
  config.gem "nokogiri", :version => '1.4.1', :lib => false
  config.gem "prism", :version => '0.1.0', :lib => false
  config.gem "rubyzip", :version => '0.9.4', :lib => false
  config.gem "columnize", :version => '0.3.1', :lib => false
  config.gem "garb", :version => '0.8.4', :lib => false
  
  #Development gems
  config.gem "populator", :version => '1.0.0', :lib => false
  config.gem "faker", :version => '0.3.1', :lib => false


  # Only load the plugins named here, in the order given (default is alphabetical).
  # :all can be used as a placeholder for all plugins not explicitly named
  config.plugins = [ :ultrasphinx, :simple_captcha, :permalink_fu, :all ]

  # Skip frameworks you're not going to use. To use Rails without a database,
  # you must remove the Active Record framework.
  # config.frameworks -= [ :active_record, :active_resource, :action_mailer ]

  # Activate observers that should always be running
  config.active_record.observers = :user_observer, :admission_observer

  # Set Time.zone default to the specified zone and make Active Record auto-convert to this zone.
  # Run "rake -D time" for a list of tasks for finding time zone names.
  config.time_zone = 'Madrid'

  # The default locale is :en and all translations from config/locales/*.rb,yml are auto loaded.
  # config.i18n.load_path += Dir[Rails.root.join('my', 'locales', '*.{rb,yml}')]
  # config.i18n.default_locale = :de
 
end
