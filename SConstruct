
import os

optimized = True

env = Environment(ENV=os.environ)

#env.Append(CFLAGS=Split('-g'))
#env.Append(CPPFLAGS=Split('-g'))

flags = []
if env['PLATFORM'] == 'posix':
	flags.extend(['-g', '-Wall'])
elif env['PLATFORM'] == 'win32':
	env.Append(CPPDEFINES=Split('_CRT_SECURE_NO_WARNINGS'))
	env.Append(LINKFLAGS=Split('/INCREMENTAL:NO'))
	if optimized:
		env.Append(CPPDEFINES=Split('NDEBUG'))
		flags.extend(['/Ox'])

	env.Append(LINKFLAGS=Split('/DEBUG'))
	flags.extend(['/Z7', '/EHsc', '/W4', '/wd4127'])

env.Append(CFLAGS=flags)
env.Append(CPPFLAGS=flags)

env.Append(CPPPATH='#lua/src')
env.Program('tundra', Glob('src/*.c') + Glob('src/*.cpp') + ['lua/src/luacpp.cpp'])
