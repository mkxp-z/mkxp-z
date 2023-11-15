win = import('windows')

res = files(
windows_resource_directory + '/resource.h',
windows_resource_directory + '/icon.ico',
windows_resource_directory + '/mkxpz.manifest',
windows_resource_directory + '/resource.rc'
)

windows_resources = win.compile_resources(windows_resource_directory + '/resource.rc', depend_files: res)
