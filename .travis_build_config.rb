MRuby::Build.new do |conf|
  toolchain :gcc
  conf.gembox 'default'
  conf.gem File.expand_path(File.dirname(__FILE__))
  conf.enable_test

  conf.gem core: 'mruby-io' if ENV['EXAMPLE']

  conf.enable_debug
  conf.gem mgem: 'mruby-errno'
  if ENV['DEBUG'] == 'true'
    conf.cc.defines = %w(MRB_ENABLE_DEBUG_HOOK)
    conf.gem core: 'mruby-bin-debugger'
  end
end
