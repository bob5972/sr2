# meson.build -- part of SpaceRobots2
# "Look on my works, ye Mighty, and despair!" --Ozymandias

project('sr2', 'c', 'cpp')

MBLib_proj = subproject('MBLib')
MBLib_dep = MBLib_proj.get_variable('MBLib_dep')

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
    'display.c',
]

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

src = engineSrc + fleetSrc

executable('sr2', sources : src,
           dependencies : MBLib_dep)
