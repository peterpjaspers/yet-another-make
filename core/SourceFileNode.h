#pragma once
#include "FileNode.h"

namespace  YAM
{
	class __declspec(dllexport) SourceFileNode : public FileNode
	{
		SourceFileNode(ExecutionContext* context, std::filesystem::path const& name);
	};
}
