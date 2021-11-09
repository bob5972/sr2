# meson.build -- part of SpaceRobots2
# "Look on my works, ye Mighty, and despair!" --Ozymandias

project('sr2', 'c', 'cpp')

engineSrc = [
    'main.c',
    'battle.c',
    'fleet.c',
    'fleetUtil.c',
    'mob.c',
    'sprite.c',
    'workQueue.c',
    'mobSet.cpp',
    'sensorGrid.cpp',
    'shipAI.cpp',
]

if get_option('gui')
    engineSrc += ['display.c']
endif

fleetSrc = [
    'cloudFleet.c',
    'dummyFleet.c',
    'runAwayFleet.c',
    'gatherFleet.c',
    'mapperFleet.c',
    'simpleFleet.c',
    'basicFleet.cpp',
    'bobFleet.cpp',
    'circleFleet.cpp',
    'cowardFleet.cpp',
    'flockFleet.cpp',
    'holdFleet.cpp',
]

mysrc = engineSrc + fleetSrc

cargs = [
    '-D_GNU_SOURCE',
    '-march=native',
]

if get_option('devel')
    message('Enabling devel options')
    cargs += ['-Wall', '-Wextra', '-Werror']
    if host_machine.system() != 'darwin'
        cargs += ['-Wno-format-truncation']
    endif
else
    message('Disabling devel options')
endif

cargs += ['-Wno-attributes']
cargs += ['-Wno-unused-parameter']
cargs += ['-Wno-sign-compare']
cargs += ['-Wno-unused-result']

if get_option('debug')
    message('Enabling debug options')
    cargs += ['-g', '-Og']
    cargs += ['-fno-omit-frame-pointer']
    cargs += ['-Wno-type-limits']
else
    message('Disabling debug options')
    cargs += ['-O2']
    cargs += ['-fomit-frame-pointer']
    cargs += ['-Wno-unused-variable']
    cargs += ['-Wno-unused-function']
endif

if host_machine.system() == 'darwin'
    cargs += ['-Wno-constant-logical-operand']
    cargs += ['-Wno-unused-function']
endif

# Configuration
myconf = []
if get_option('gui')
    myconf += ['SR2_GUI']
endif
conf_data = configuration_data()
foreach c : myconf
    conf_data.set(c, 1)
endforeach
configure_file(input : 'SR2Config.h.in',
               output : 'SR2Config.h',
               configuration : conf_data)

# Language arguments
add_project_arguments(cargs, language : 'c')
add_project_arguments(cargs, language : 'cpp')

MBLib_proj = subproject('MBLib')
MBLib_dep = MBLib_proj.get_variable('MBLib_dep')

SDL2_dep = dependency('SDL2')
SDL2ttf_dep = dependency('SDL2_ttf')
libpng_dep = dependency('libpng')

mydeps = [MBLib_dep, SDL2_dep, SDL2ttf_dep, libpng_dep]

executable('sr2', sources : mysrc,
           dependencies : mydeps)
