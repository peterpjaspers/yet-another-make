#pragma once
#include "Node.h"

namespace YAM
{
	// TODO: analyze the following proposal 
	//    - create SourceDirectoryNode and FileNode to cache directory tree
	//      This means that, unlike proposed in design_whenToAddSourceFileNodesToGraph,
	//      it is not possible to only cache parts of a directory tree.
	//    - create GlobNode on a SourceDirectoryNode
	//      E.g. glob  /cygdrive/d/Peter/github/yam/*/*Node.cpp is created
	//      on /cygdrive/d/Peter/github/yam with glob */*Node.cpp
	// 
	// A glob pattern specifies a sets of filenames with wildcard characters.
	// E.g *.txt specifies all text files in the current directory.
	//
	// A glob pattern can contain the following wildcards:
	//    *  matches any number of any characters including none 
	//    ?  matches any single character
	//	  ** matches all files and zero or more directories and subdirectories. 
	//       If the pattern is followed by a /, only directories and subdirectories match.
	//       A glob that contains ** is called a recursive glob.
	// 
	// Globs do not match invisible files like ., .., .git, .rc unless the dot (.) is
	// specified in the glob pattern.
	// 
	// Glob examples:
	//    *.cpp matches all visible cpp files in the current directory
	//    .* matches all visible and invisible files in the current directory
	//    /root/cor*/*.cpp matches all cpp files in directories whose names match /root/cor* 
	//    /root/**/*.cpp matches all cpp files in all directory trees in /root
	// 
	// A GlobNode is capable of:
	//    - evaluating a non-recursive glob, e.g. *.package
	//      For each matching file/directory it creates a file node.
	//    - evaluating a recursive glob, e.g. /root/sub/**/*.package
	//      For each matching file it creates a file node.
	//      For each matching directory it creates a recursive glob node
	//	  - detecting changes in the glob pattern and directory content.
	//      Such changes will out-date the glob evaluation result (i.e.
	//      the created file/glob nodes. 
	//
	class __declspec(dllexport) GlobNode : public Node
	{
	};
}

