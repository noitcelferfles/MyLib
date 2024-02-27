local_source_files = [
	'tx_assert.c', 
	'tx_automemory.cpp', 
	'tx_memory.cpp', 
	'tx_memory_halffit.cpp',
	]

foreach local_source_file : local_source_files
    source_files += ['@0@/@1@'.format(meson.current_source_dir(), local_source_file)]
endforeach


header_directories += include_directories('.')