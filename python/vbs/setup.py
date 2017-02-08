#!/usr/bin/env python 

from distutils.core import setup, Extension

files = ["src/_vbs.cpp", "src/blob.c", ]

base_dir = "../.."

_vbsExt = Extension("_vbs", 
		sources = files, 
                include_dirs = ["/usr/local/include", base_dir],
                library_dirs = ["/usr/local/lib", base_dir + "/lib"],
		libraries = ["xs"],
	)

setup(
	name = "vbs",  
	version = "1.0",  
	description = "vbs module",  
	author = "xiong jiagui",
	author_email = "xiongjg@tsinghua.org.cn",
	ext_modules = [_vbsExt],
	py_modules = ["vbs"],
)

