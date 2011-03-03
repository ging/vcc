# Copyright (c) 2006-2010 Nick Sieger <nicksieger@gmail.com>
# See the file LICENSE.txt included with the distribution for
# software license details.

require 'ci/reporter/core'
tried_gem = false
begin
  require 'cucumber'
  require 'cucumber/ast/visitor'
rescue LoadError
  unless tried_gem
    tried_gem = true
    require 'rubygems'
    gem 'cucumber'
    retry
  end
end

module CI
  module Reporter
    class CucumberFailure
      attr_reader :step

      def initialize(step)
        @step = step
      end

      def failure?
        true
      end

      def error?
        !failure?
      end

      def name
        step.exception.class.name
      end

      def message
        step.exception.message
      end

      def location
        step.exception.backtrace.join("\n")
      end
    end

    class Cucumber < ::Cucumber::Ast::Visitor

      attr_accessor :test_suite, :report_manager, :feature_name

      def initialize(step_mother, io, options)
        self.report_manager = ReportManager.new("features")
        super(step_mother)
      end

      def visit_feature_name(name)
        self.feature_name = name.split("\n").first
        super
      end

      def visit_feature_element(feature_element)
        self.test_suite = TestSuite.new("#{feature_name} #{feature_element.instance_variable_get("@name")}")
        test_suite.start

        return_value = super

        test_suite.finish
        report_manager.write_report(test_suite)
        self.test_suite = nil

        return_value
      end

      def visit_step(step)
        test_case = TestCase.new(step.name)
        test_case.start

        return_value = super

        test_case.finish

        case step.status
        when :pending, :undefined
          test_case.name = "#{test_case.name} (PENDING)"
        when :skipped
          test_case.name = "#{test_case.name} (SKIPPED)"
        when :failed
          test_case.failures << CucumberFailure.new(step)
        end

        test_suite.testcases << test_case

        return_value
      end
    end
  end
end
