require_relative 'mrblib/fiberer_worker/version'

MRuby::Gem::Specification.new('mruby-fibered_worker') do |spec|
  spec.license = 'MIT'
  spec.authors = 'Uchio Kondo'
  spec.version = FiberedWorker::VERSION

  spec.add_dependency 'mruby-time', core: 'mruby-time'
  spec.add_dependency 'mruby-fiber', core: 'mruby-fiber'
  spec.add_dependency 'mruby-process', mgem: 'mruby-process'

  spec.add_dependency 'mruby-sleep', core: 'mruby-sleep'

  spec.linker.libraries << 'rt'
end
