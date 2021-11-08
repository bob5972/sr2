# sample/meson.build -- part of MBLib
# "Look on my works, ye Mighty, and despair!" --Ozymandias

project('sample', 'c', 'cpp')

MBLib_proj = subproject('MBLib')
MBLib_dep = MBLib_proj.get_variable('MBLib_dep')

executable('sample', 'main.c',
           dependencies : MBLib_dep)
