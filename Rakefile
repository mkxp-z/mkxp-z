require 'rake/testtask'

Rake::TestTask.new do |t|
  t.libs << "tests"
  t.test_files = FileList['tests/**/*_test.rb']
  t.verbose = true
end

task :default => :test