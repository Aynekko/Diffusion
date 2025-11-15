#! /usr/bin/env python
# encoding: utf-8
# a1batross, mittorn, 2018

from waflib import Build, Configure, Context, Logs, TaskGen
import sys
import os
import re

VERSION = '1.0'
APPNAME = 'Diffusion'
top = '.'
default_prefix = '/'

Context.Context.line_just = 60 # should fit for everything on 80x26

@Configure.conf
def get_taskgen_count(self):
	try: idx = self.tg_idx_count
	except: idx = 0 # don't set tg_idx_count to not increase counter
	return idx

@TaskGen.feature('cshlib', 'cxxshlib', 'fcshlib')
@TaskGen.before_method('apply_implib')
def remove_implib_install(self):
	if not getattr(self, 'install_path_implib', None):
		self.install_path_implib = None

def options(opt):
	opt.load('reconfigure compiler_optimizations compiler_cxx compiler_c clang_compilation_database strip_on_install subproject')

	grp = opt.add_option_group('Common options')

	grp.add_option('-4', '--32bits', action = 'store_true', dest = 'FORCE32', default = False,
		help = 'force targetting 32-bit libs, usually unneeded [default: %(default)s]')
	grp.add_option('--disable-werror', action = 'store_true', dest = 'DISABLE_WERROR', default = False,
		help = 'disable compilation abort on warning')

	opt.add_subproject('client server 3rd-party/mainui_cpp')

def configure(conf):
	conf.env.EXPORT_DEFINES_LIST = []
	conf.load('fwgslib reconfigure compiler_optimizations')

	# Load compilers early
	conf.load('compiler_c compiler_cxx')

	# HACKHACK: override msvc DEST_CPU value by something that we understand
	if conf.env.DEST_CPU == 'amd64':
		conf.env.DEST_CPU = 'x86_64'

	conf.load('subproject clang_compilation_database strip_on_install enforce_pic')

	conf.check_pic(True) # modern defaults

	conf.env.BIT32_MANDATORY = conf.options.FORCE32

	if conf.env.BIT32_MANDATORY:
		Logs.info('WARNING: will build game for 32-bit target')

	conf.load('force_32bit')

	cflags, linkflags = conf.get_optimization_flags()
	cxxflags = list(cflags) # optimization flags are common between C and C++ but we need a copy

	conf.check_cc(cflags=cflags, linkflags=linkflags, msg='Checking for required C flags')
	conf.check_cxx(cxxflags=cxxflags, linkflags=linkflags, msg='Checking for required C++ flags')

	conf.env.append_unique('CFLAGS', cflags)
	conf.env.append_unique('CXXFLAGS', cxxflags)
	conf.env.append_unique('LINKFLAGS', linkflags)

	if conf.env.COMPILER_CC != 'msvc' and not conf.options.DISABLE_WERROR:
		opt_flags = [
			# '-Wall', '-Wextra', '-Wpedantic',
			'-fdiagnostics-color=always',

			# stable diagnostics, forced to error, sorted
			'-Werror=bool-compare',
			'-Werror=bool-operation',
			'-Werror=cast-align=strict',
			'-Werror=duplicated-cond',
			# '-Werror=format=2',
			'-Werror=implicit-fallthrough=2',
			# '-Werror=logical-op',
			'-Werror=packed',
			'-Werror=packed-not-aligned',
			#'-Werror=parentheses',
			'-Werror=return-type',
			'-Werror=sequence-point',
			'-Werror=sizeof-pointer-memaccess',
			'-Werror=sizeof-array-div',
			'-Werror=sizeof-pointer-div',
			# '-Werror=strict-aliasing',
			'-Werror=string-compare',
			'-Werror=tautological-compare',
			'-Werror=use-after-free=3',
			'-Werror=vla',
			'-Werror=write-strings',

			# unstable diagnostics, may cause false positives
			'-Winit-self',
			'-Wmisleading-indentation',
			'-Wunintialized',

			# disabled, flood
			# '-Wdouble-promotion',
		]

		opt_cflags = [
			'-Werror=declaration-after-statement',
			'-Werror=enum-conversion',
			'-Werror=implicit-int',
			'-Werror=implicit-function-declaration',
			'-Werror=incompatible-pointer-types',
			'-Werror=int-conversion',
			'-Werror=jump-misses-init',
			# '-Werror=old-style-declaration',
			# '-Werror=old-style-definition',
			# '-Werror=strict-prototypes',
			'-fnonconst-initializers' # owcc
		]

		opt_cxxflags = [] # TODO:

		cflags = conf.filter_cflags(opt_flags + opt_cflags, cflags)
		cxxflags = conf.filter_cxxflags(opt_flags + opt_cxxflags, cxxflags)

		conf.env.append_unique('CFLAGS', cflags)
		conf.env.append_unique('CXXFLAGS', cxxflags)

	conf.check_cc(lib='m')

	# check if we can use C99 tgmath
	if conf.check_cc(header_name='tgmath.h', mandatory=False):
		if conf.env.COMPILER_CC == 'msvc':
			conf.define('_CRT_SILENCE_NONCONFORMING_TGMATH_H', 1)
		tgmath_usable = conf.check_cc(fragment='''#include<tgmath.h>
			const float val = 2, val2 = 3;
			int main(void){ return (int)(-asin(val) + cos(val2)); }''',
			msg='Checking if tgmath.h is usable', mandatory=False, use='M')
		conf.define_cond('HAVE_TGMATH_H', tgmath_usable)
	else:
		conf.undefine('HAVE_TGMATH_H')
	cmath_usable = conf.check_cxx(fragment='''#include<cmath>
			int main(void){ return (int)sqrt(2.0f); }''',
			msg='Checking if cmath is usable', mandatory = False)
	conf.define_cond('HAVE_CMATH', cmath_usable)

	conf.env.append_unique('CXXFLAGS', ['-Wno-invalid-offsetof', '-fno-exceptions'])
	conf.define('stricmp', 'strcasecmp', quote=False)
	conf.define('strnicmp', 'strncasecmp', quote=False)
	conf.define('_snprintf', 'snprintf', quote=False)
	conf.define('_vsnprintf', 'vsnprintf', quote=False)
	conf.define('_LINUX', True)
	conf.define('LINUX', True)

	conf.msg(msg='-> processing mod options', result='...', color='BLUE')
	regex = re.compile('^([A-Za-z0-9_-]+)=([A-Za-z0-9_-]+) # (.*)$')
	with open(str(conf.path.make_node('mod_options.txt'))) as fd:
		lines = fd.readlines()
	for line in lines:
		m = regex.match(line.strip())
		if m:
			p = m.groups()
			conf.start_msg("* " + p[2])
			if p[1] == 'ON':
				conf.env[p[0]] = True
				conf.define(p[0], 1)
			elif p[1] == 'OFF':
				conf.env[p[0]] = False
				conf.undefine(p[0])
			else:
				conf.env[p[0]] = p[1]
			conf.end_msg(p[1])
	if conf.env.HLDEMO_BUILD and conf.env.OEM_BUILD:
		conf.fatal('Don\'t mix Demo and OEM builds!')

	# strip lib from pattern
	if conf.env.cxxshlib_PATTERN.startswith('lib'):
		conf.env.cxxshlib_PATTERN = conf.env.cxxshlib_PATTERN[3:]

	conf.load('library_naming')
	conf.add_subproject('client server 3rd-party/mainui_cpp')

def build(bld):
	if bld.is_install and not bld.options.destdir:
		bld.fatal('Set the install destination directory using --destdir option')

	# don't clean QtCreator files and reconfigure saved options
	bld.clean_files = bld.bldnode.ant_glob('**',
		excl='*.user configuration.py .lock* *conf_check_*/** config.log %s/*' % Build.CACHE_DIR,
		quiet=True, generator=True)

	bld.add_subproject('client server 3rd-party/mainui_cpp')
